# Firmware

STM32U585 firmware for the B-U585I-IOT02A development board, built with FreeRTOS.

## Building and flashing

### STM32CubeIDE

1. Open STM32CubeIDE and import the `firmware/` directory using **File > Import > General > Existing Projects into Workspace**.
2. Build the project using **Project > Build Project**.
3. Connect the board to your computer through the ST-LINK USB port (CN8).
4. Right-click the project and select **Run As > STM32 C/C++ Application** to flash the board, or **Debug As > STM32 C/C++ Application** to start debugging.

STM32CubeMX is used to configure peripherals and generate initialization code, while STM32CubeIDE handles building, flashing, and debugging.

### Command line (CMake)

You'll need CMake 3.25+, Ninja, and the GNU Arm toolchain (`arm-none-eabi-gcc`).

Make sure the compiler is available in your `PATH`, or set `ARM_TOOLCHAIN_BIN` to its `bin` directory.

From the repository root:

```bash
cd firmware
cmake --preset debug
cmake --build --preset debug
```

Build outputs are placed in `build/debug/` at the repository root. For an optimized build, replace `debug` with `release`.

To flash the firmware, use STM32CubeProgrammer with ST-LINK and write `build/debug/stm32_telemetry.bin` to address `0x08000000`. Reset the board after flashing.

## Project structure

| Folder | Description |
|---|---|
| `Core/` | CubeMX-generated initialization and startup code |
| `Drivers/` | STM32 HAL and CMSIS drivers |
| `Middlewares/` | FreeRTOS and CMSIS-RTOS |
| `App/` | Application code, including data acquisition and logging |
| `BSP/` | Board-specific hardware interfaces |
| `../lib/` | Shared drivers and libraries, linked as `Lib` in CubeIDE |

## Design decisions

- As HAL, CubeMX output and FreeRTOS are all in C and most embedded code is written in C it would be good to continue with it when possible. If C++ will bring large benefits then it can begin being used, especially as it's easy to add.
	- Begin with `struct`s
- FreeRTOS is being used to ensure data isn't dropped. Different data sources operate at different frequencies so being able to switch to logging that data from another task will ensure it isn't dropped.
    - FreeRTOS was selected specifically due to familiarity and meets requirements
- Heap_1 was selected for the RTOS as dynamic memory allocation does need seem necessary at the moment