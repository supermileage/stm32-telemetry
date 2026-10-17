get_filename_component(_repo "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(_firmware "${_repo}/firmware")
set(_libraries "${_repo}/lib")
add_library(host_fatfs STATIC "${_libraries}/fatfs/upstream/src/ff.c"
    "${_libraries}/fatfs/upstream/src/diskio.c" "${_libraries}/fatfs/upstream/src/ff_gen_drv.c")
target_include_directories(host_fatfs SYSTEM PUBLIC "${_libraries}/fatfs")

add_subdirectory("${_libraries}/drivers/sd_spi" "${CMAKE_CURRENT_BINARY_DIR}/sd_spi")
target_link_libraries(sd_spi PRIVATE telemetry_warnings)
add_executable(sd_spi_tests sd_spi/test_sd_spi.c
    "${_firmware}/BSP/sd_spi_port.cpp"
    "${_firmware}/BSP/b_u585i_iot02a/board_sd.cpp"
    "${_libraries}/drivers/sd_spi/fatfs/sd_diskio.c"
    "${_firmware}/app/storage/storage.c"
    "${_firmware}/app/logging/log_file.c")
target_include_directories(sd_spi_tests PRIVATE sd_spi "${_libraries}"
    "${_libraries}/drivers/sd_spi/fatfs"
    "${_firmware}/BSP" "${_firmware}/BSP/b_u585i_iot02a"
    "${_firmware}/app/storage" "${_firmware}/app/logging")
target_link_libraries(sd_spi_tests PRIVATE sd_spi host_fatfs telemetry_warnings)
target_compile_features(sd_spi_tests PRIVATE cxx_std_17)
set_target_properties(sd_spi_tests PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(sd_spi_tests PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
add_test(NAME sd_spi_and_fatfs COMMAND sd_spi_tests)

add_executable(sd_port_tests sd_spi/test_port_callbacks.c)
target_link_libraries(sd_port_tests PRIVATE sd_spi telemetry_warnings)
add_test(NAME sd_port_callbacks COMMAND sd_port_tests)

add_executable(sd_cpp_port_tests sd_spi/test_sd_spi_port.cpp
    "${_firmware}/BSP/sd_spi_port.cpp"
    "${_firmware}/BSP/b_u585i_iot02a/board_sd.cpp")
target_include_directories(sd_cpp_port_tests PRIVATE sd_spi
    "${_libraries}/drivers/sd_spi" "${_firmware}/BSP" "${_firmware}/BSP/b_u585i_iot02a")
target_compile_features(sd_cpp_port_tests PRIVATE cxx_std_17)
set_target_properties(sd_cpp_port_tests PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(sd_cpp_port_tests PRIVATE -UNDEBUG -fno-exceptions -fno-rtti)
target_link_libraries(sd_cpp_port_tests PRIVATE sd_spi telemetry_warnings)
add_test(NAME sd_spi_port_cpp COMMAND sd_cpp_port_tests)

add_executable(logger_tests logging/test_logger.c logging/rtos_mock.c
    "${_firmware}/app/logging/logger.c")
target_include_directories(logger_tests PRIVATE logging "${_firmware}/app/logging")
target_link_libraries(logger_tests PRIVATE host_fatfs telemetry_warnings)
add_test(NAME logger_queue COMMAND logger_tests)
add_test(NAME logger_error_state COMMAND logger_tests failure)

add_executable(log_file_tests logging/test_log_file.c "${_firmware}/app/logging/log_file.c")
target_include_directories(log_file_tests PRIVATE "${_firmware}/app/logging"
    "${_libraries}/fatfs")
target_link_libraries(log_file_tests PRIVATE telemetry_warnings)
add_test(NAME log_file_failures COMMAND log_file_tests)

add_executable(storage_task_tests logging/test_storage_task.c logging/rtos_mock.c
    "${_firmware}/app/logging/logger.c" "${_firmware}/app/storage/storage_task.c")
target_include_directories(storage_task_tests PRIVATE logging "${_firmware}/app"
    "${_firmware}/app/logging")
target_link_libraries(storage_task_tests PRIVATE host_fatfs telemetry_warnings)
foreach(_scenario IN ITEMS normal flush idle mount_failure verify_failure open_failure
        append_failure sync_failure close_failure unmount_failure task_failure
        hello hello_mount_failure hello_verify_failure hello_unmount_failure hello_task_failure)
    add_test(NAME storage_task_${_scenario} COMMAND storage_task_tests ${_scenario})
endforeach()

foreach(_test IN ITEMS sd_spi_tests sd_port_tests logger_tests log_file_tests storage_task_tests)
    target_compile_features(${_test} PRIVATE c_std_11)
    # assert() is the test harness; keep it enabled under any configuration.
    target_compile_options(${_test} PRIVATE -UNDEBUG)
endforeach()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND EXISTS "${_repo}/tools/log_decoder.py")
    add_test(NAME log_decoder COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/logging/test_decode_log.py")
endif()
