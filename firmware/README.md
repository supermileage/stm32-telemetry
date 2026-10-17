# Firmware

STM32U585 firmware for the B-U585I-IOT02A board, using FreeRTOS. Application
startup and board support use C++17 with exceptions and RTTI disabled.
Generated code, HAL, FreeRTOS, FatFs, and the SD protocol driver remain C.

## Build and flash

Initialize the submodules as described in the [repository setup](../README.md#build-and-run).

In STM32CubeIDE:

1. Import `firmware/` using **File > Import > General > Existing Projects into Workspace**.
2. Select Debug or Release and use **Project > Build Project**.
3. Connect the board through its ST-LINK USB port (CN8).
4. Select **Run As > STM32 C/C++ Application** to flash, or **Debug As** to debug.

For CMake, install CMake 3.25+, Ninja, and the GNU Arm toolchain. Put
`arm-none-eabi-gcc` and the other Arm tools on PATH, or set `ARM_TOOLCHAIN_BIN`
to their `bin` directory. From the repository root:

```bash
cd firmware
cmake --preset debug
cmake --build --preset debug
```

Outputs go to `build/debug/` at the repository root. Replace `debug` with
`release` for an optimized build. Flash `stm32_telemetry.bin` to `0x08000000`
using STM32CubeProgrammer and ST-LINK, then reset the board.

## Current behavior

`MX_FREERTOS_Init()` calls `app_init()` after peripheral setup and before the
scheduler starts. It binds the SD driver to SPI1 and PE12, leaves CS high,
and selects a startup clock at or below 400 kHz. A configuration failure
enters `Error_Handler()`. Binding sends no card commands; the generated
default task then idles under FreeRTOS.

The C storage and logger modules are covered by [host tests](../tests/README.md),
but startup does not call them and CMake's firmware target does not include
their sources. The `sd-hello` preset sets `TELEMETRY_SD_HELLO_TEST`, which is
not used by startup yet. Building that preset does not run a card test.

## Source layout

| Folder | Contents |
| --- | --- |
| `Core/` | CubeMX initialization, startup, and interrupt code |
| `Drivers/` | STM32 HAL and CMSIS |
| `Middlewares/` | FreeRTOS and CMSIS-RTOS |
| [app/](app/README.md) | Application startup, storage, and logging |
| [BSP/](BSP/README.md) | Board wiring and HAL adapters |
| `../lib/` | Shared drivers and libraries, linked as `Lib` in CubeIDE |

Keep generated-file edits in `USER CODE` blocks. CubeIDE and CMake have
separate build settings; review both after CubeMX regeneration. The main
firmware uses FreeRTOS `heap_1`, so tasks must not delete themselves. The
storage task uses static allocation and idles after shutdown.
