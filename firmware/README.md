# Firmware

This folder contains the STM32 firmware for the **B-U585I-IOT02A** development board, built using **STM32CubeIDE**, **STM32 HAL**, and **FreeRTOS**.

The project is organized to keep application code separate from hardware-specific code, making it easier to add new features, integrate peripherals, and maintain the firmware.

## Build and Flash

Before building, install STM32CubeIDE and initialize the repository submodules. See the [repository setup](../README.md#build-and-run).

1. Open STM32CubeIDE.
2. Go to **File > Import > General > Existing Projects into Workspace** and select `firmware/`.
3. Choose the Debug or Release configuration and build the project.
4. Connect the board using the ST-LINK USB port (CN8).
5. Use **Run As > STM32 C/C++ Application** to flash or **Debug As** to debug.

Build outputs are stored in `Debug/` or `Release/`.

To generate a `.bin` file, enable **MCU/MPU Post build outputs > Convert to binary file (-O binary)** in the project settings.

## Project Structure

| Folder | Description |
|---|---|
| `Core/` | CubeMX-generated initialization, interrupts, and RTOS setup |
| `Drivers/` | STM32 HAL and CMSIS drivers |
| `Middlewares/` | FreeRTOS and other middleware |
| `app/` | Application logic, tasks, and features |
| `BSP/` | Board-specific drivers and hardware interfaces |
| `../lib/` | Shared and reusable libraries |

### Application (`app/`)

Contains the main application code, including tasks, sensor handling, data processing, and other application features.

Application initialization starts in `app.cpp`.

### Board Support Package (`BSP/`)

Contains hardware-specific code and interfaces between the application and STM32 peripherals.

This keeps hardware details separate from application logic and makes drivers easier to reuse.

## Firmware Startup

1. `main()` initializes the MCU, clocks, and peripherals.
2. `MX_FREERTOS_Init()` initializes the RTOS and calls `app_init()`.
3. `app_init()` sets up application resources and tasks.
4. The FreeRTOS scheduler starts and the application tasks begin running.
