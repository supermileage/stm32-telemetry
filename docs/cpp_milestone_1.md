# C++ startup and build support

The first C++ milestone adds `app.cpp` and calls it from generated C RTOS
initialization. The build settings and startup bridge are already in place.
The next stage added SD SPI configuration to `app_init()`; it is no longer
an empty stub. See the [BSP guide](../firmware/BSP/README.md) for that code.

## C and C++ boundary

`firmware/app/app.h` declares `app_init()` inside `extern "C"` guards for
C++ callers. Generated `app_freertos.c` includes that header in its
`USER CODE` include block and calls the function in `MX_FREERTOS_Init()`.

`app.cpp` defines the function with C linkage and checks that it is compiled
as C++17. C linkage keeps the symbol callable from C; the body is still C++
and can use C++ functions and objects.

`main.c` initializes peripherals and the RTOS kernel before calling
`MX_FREERTOS_Init()`, then starts the scheduler. Application initialization
must not wait for a task to run before the scheduler starts. The current
function binds the SD SPI port and enters `Error_Handler()` on failure.

Keep generated `.c` files, HAL, FreeRTOS, and FatFs as C. Adding C++ application
code does not require changing peripheral settings or regenerating CubeMX
output. User-code preservation is enabled in the `.ioc`; keep generated-file
edits in `USER CODE` blocks. Review build files after regeneration.

## Build settings

CMake and CubeIDE have separate settings. Both Debug and Release need:

- C++17 for application and BSP `.cpp` files, with exceptions and RTTI disabled
  (`-fno-exceptions -fno-rtti`).
- The same CPU, floating-point ABI, include paths, and device defines as the C
  build where those dependencies are used.
- The C++ driver for the final link and the existing `STM32U585AIIXQ_FLASH.ld`
  linker script.

CMake declares `LANGUAGES C CXX`, lists the application sources explicitly,
and finds `arm-none-eabi-g++` in the Arm toolchain file. The host-test preset
uses native `gcc` and `g++`. New source files must be added to the appropriate
CMake target.

In CubeIDE, refresh the project, then check **Properties > C/C++ Build >
Settings** for the G++ compiler and linker in both configurations. The project
already has C++ support; there is no need to convert generated files. Check
the build console if the IDE settings and compiler command disagree.

## Verify startup

Follow the [firmware build guide](../firmware/README.md). From `firmware/`:

```bash
cmake --preset debug
cmake --build --preset debug
cmake --preset release
cmake --build --preset release
```

Check that `.cpp` files compile with `arm-none-eabi-g++`, generated `.c` files
still use the C compiler, and the final C++ link succeeds. On the board, set
breakpoints at `app_init()` and `StartDefaultTask()`. Reaching the second
breakpoint after continuing confirms that the scheduler started. Firmware
builds and host tests do not verify board execution.

For an undefined `app_init`, check the C linkage and source list. Multiple
definitions usually mean both old and new implementations are being built.
If `app.h` is missing, add the `app` include directory to the C compiler used
for `app_freertos.c`, as well as to the C++ compiler.
