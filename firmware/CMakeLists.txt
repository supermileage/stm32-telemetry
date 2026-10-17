cmake_minimum_required(VERSION 3.25)
project(stm32_telemetry VERSION 0.1.0 LANGUAGES C CXX)

option(TELEMETRY_BUILD_FIRMWARE "Build the STM32U585 firmware" ON)
option(TELEMETRY_SD_HELLO_TEST "Run the one-shot SD Hello World test" OFF)
option(BUILD_TESTING "Build native host tests" OFF)
option(TELEMETRY_WARNINGS_AS_ERRORS "Treat application warnings as errors" ON)
option(TELEMETRY_BUILD_DOCS "Enable the application and BSP API documentation target" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(TELEMETRY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(LIB_ROOT "${TELEMETRY_ROOT}/lib")

if(TELEMETRY_BUILD_DOCS)
    find_package(Doxygen 1.9.8 REQUIRED COMPONENTS doxygen)
    add_custom_target(docs
        COMMAND Doxygen::doxygen "${TELEMETRY_ROOT}/Doxyfile"
        WORKING_DIRECTORY "${TELEMETRY_ROOT}"
        COMMENT "Generating application and BSP API documentation"
        VERBATIM)
endif()

add_library(telemetry_warnings INTERFACE)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(telemetry_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
        $<$<BOOL:${TELEMETRY_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

if(BUILD_TESTING)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Host tests need a native compiler; use the host-tests preset.")
    endif()
    enable_testing()
    add_subdirectory("${TELEMETRY_ROOT}/tests" "${CMAKE_CURRENT_BINARY_DIR}/tests")
endif()

if(NOT TELEMETRY_BUILD_FIRMWARE)
    return()
endif()
if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Firmware requires GNU Arm; use the debug or release preset.")
endif()
enable_language(ASM)
set(CUBE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(_hal "${CUBE_ROOT}/Drivers/STM32U5xx_HAL_Driver")
set(_rtos "${CUBE_ROOT}/Middlewares/Third_Party/FreeRTOS/Source")
set(_port "${_rtos}/portable/GCC/ARM_CM33_NTZ/non_secure")
set(_linker "${CUBE_ROOT}/STM32U585AIIXQ_FLASH.ld")

foreach(_required IN ITEMS "${_linker}" "${_hal}/Src/stm32u5xx_hal.c"
        "${_port}/port.c" "${LIB_ROOT}/fatfs/upstream/src/ff.c")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Missing dependency: ${_required}. Initialize submodules and CubeMX sources.")
    endif()
endforeach()

add_library(stm32_platform INTERFACE)
target_compile_features(stm32_platform INTERFACE c_std_11)
target_compile_definitions(stm32_platform INTERFACE USE_HAL_DRIVER USE_FULL_LL_DRIVER STM32U585xx
    $<$<CONFIG:Debug>:DEBUG>)
target_compile_options(stm32_platform INTERFACE
    -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og;-g3>
    $<$<CONFIG:Release>:-Os;-g3>)
target_link_options(stm32_platform INTERFACE
    -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
    --specs=nano.specs --specs=nosys.specs)
target_include_directories(stm32_platform INTERFACE "${CUBE_ROOT}/Core/Inc")
target_include_directories(stm32_platform SYSTEM INTERFACE
    "${_hal}/Inc" "${_hal}/Inc/Legacy"
    "${CUBE_ROOT}/Drivers/CMSIS/Include"
    "${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32U5xx/Include"
    "${_rtos}/include" "${_port}" "${_rtos}/CMSIS_RTOS_V2"
    "${CUBE_ROOT}/Middlewares/Third_Party/CMSIS/RTOS2/Include")

# Explicit manifests prevent backups, examples and extra RTOS ports entering a build.
add_library(stm32_hal STATIC)
set(_hal_modules hal hal_cortex hal_dma hal_dma_ex hal_exti hal_fdcan
    hal_flash hal_flash_ex hal_gpio hal_gtzc hal_i2c hal_i2c_ex hal_icache
    hal_mdf hal_ospi hal_pcd hal_pcd_ex hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex
    hal_rtc hal_rtc_ex hal_spi hal_spi_ex hal_tim hal_tim_ex hal_uart hal_uart_ex
    ll_dlyb ll_dma ll_exti ll_gpio ll_lpgpio ll_pwr ll_ucpd ll_usb ll_utils)
foreach(_module IN LISTS _hal_modules)
    target_sources(stm32_hal PRIVATE "${_hal}/Src/stm32u5xx_${_module}.c")
endforeach()
target_link_libraries(stm32_hal PUBLIC stm32_platform)

add_library(freertos STATIC
    "${_rtos}/tasks.c" "${_rtos}/queue.c" "${_rtos}/list.c"
    "${_rtos}/timers.c" "${_rtos}/event_groups.c" "${_rtos}/stream_buffer.c"
    "${_rtos}/croutine.c" "${_rtos}/CMSIS_RTOS_V2/cmsis_os2.c"
    "${_rtos}/portable/MemMang/heap_1.c" "${_port}/port.c" "${_port}/portasm.c")
target_link_libraries(freertos PUBLIC stm32_platform)

add_library(fatfs STATIC "${LIB_ROOT}/fatfs/upstream/src/ff.c"
    "${LIB_ROOT}/fatfs/upstream/src/diskio.c" "${LIB_ROOT}/fatfs/upstream/src/ff_gen_drv.c")
target_include_directories(fatfs SYSTEM PUBLIC "${LIB_ROOT}/fatfs")
target_link_libraries(fatfs PRIVATE stm32_platform)

add_subdirectory("${LIB_ROOT}/drivers/sd_spi" "${CMAKE_CURRENT_BINARY_DIR}/sd_spi")
target_link_libraries(sd_spi PRIVATE stm32_platform telemetry_warnings)

add_library(telemetry_app STATIC
    app/app.cpp
    BSP/b_u585i_iot02a/board_sd.cpp BSP/sd_spi_port.cpp)

target_compile_features(telemetry_app PRIVATE cxx_std_17)
set_target_properties(telemetry_app PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(telemetry_app PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
target_include_directories(telemetry_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/app"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BSP/b_u585i_iot02a" "${CMAKE_CURRENT_SOURCE_DIR}/BSP")
target_link_libraries(telemetry_app PUBLIC freertos sd_spi fatfs PRIVATE telemetry_warnings)

# Objects retain strong IRQ/MSP callbacks that otherwise may stay in an archive.
add_library(cube_generated OBJECT)
set(_core_sources main app_freertos gpio fdcan i2c icache mdf octospi rtc spi
    stm32u5xx_hal_msp stm32u5xx_hal_timebase_tim stm32u5xx_it system_stm32u5xx
    syscalls sysmem ucpd usart usb_otg)
foreach(_source IN LISTS _core_sources)
    target_sources(cube_generated PRIVATE "${CUBE_ROOT}/Core/Src/${_source}.c")
endforeach()
target_sources(cube_generated PRIVATE "${CUBE_ROOT}/Core/Startup/startup_stm32u585aiixq.s")
target_link_libraries(cube_generated PRIVATE stm32_platform telemetry_app)
target_compile_definitions(telemetry_app PRIVATE
    TELEMETRY_SD_HELLO_TEST=$<BOOL:${TELEMETRY_SD_HELLO_TEST}>)

add_executable(stm32_telemetry $<TARGET_OBJECTS:cube_generated>)
set_target_properties(stm32_telemetry PROPERTIES
    SUFFIX ".elf" C_EXTENSIONS OFF LINKER_LANGUAGE CXX)
target_link_libraries(stm32_telemetry PRIVATE telemetry_app stm32_hal)
target_link_options(stm32_telemetry PRIVATE "-T${_linker}"
    "LINKER:-Map,${CMAKE_CURRENT_BINARY_DIR}/stm32_telemetry.map"
    LINKER:--gc-sections LINKER:--print-memory-usage)
set_property(TARGET stm32_telemetry APPEND PROPERTY LINK_DEPENDS "${_linker}")
add_custom_command(TARGET stm32_telemetry POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:stm32_telemetry>"
        "${CMAKE_CURRENT_BINARY_DIR}/stm32_telemetry.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:stm32_telemetry>"
        "${CMAKE_CURRENT_BINARY_DIR}/stm32_telemetry.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:stm32_telemetry>"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/stm32_telemetry.hex"
        "${CMAKE_CURRENT_BINARY_DIR}/stm32_telemetry.bin"
    VERBATIM)
