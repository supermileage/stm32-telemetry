# Firmware

STM32U585 firmware for the B-U585I-IOT02A board, using FreeRTOS.

## Build and flash

Initialize the submodules as described in the [repository setup](../README.md#build-and-run).

In STM32CubeIDE:

1. Import `firmware/` using **File > Import > General > Existing Projects into Workspace**.
2. Select Debug or Release and use **Project > Build Project**.
3. Connect the board through its ST-LINK USB port (CN8).
4. Select **Run As > STM32 C/C++ Application** to flash, or **Debug As** to debug.

CubeIDE manages the firmware build through `.project` and `.cproject`. Output
goes to `firmware/Debug/` or `firmware/Release/`. Enable **MCU/MPU Post build
outputs > Convert to binary file (-O binary)** if you need a `.bin` for releases.

## Source layout

| Folder | Contents |
| --- | --- |
| `Core/` | CubeMX initialization, startup, and interrupt code |
| `Drivers/` | STM32 HAL and CMSIS |
| `Middlewares/` | FreeRTOS and CMSIS-RTOS |
| `app/` | Application startup, storage, and logging |
| `BSP/` | Board wiring and HAL adapters |
| `../lib/` | Shared drivers and libraries, linked as `Lib` in CubeIDE |

For startup behavior and ownership rules, see [architecture](../docs/architecture.md).
For wiring and the first card check, see [hardware](../docs/hardware.md) and
[storage and logging](../docs/storage_logging.md#check-the-card).

## API reference

The headers document the APIs. If Doxygen is installed, run `doxygen Doxyfile`
from the repository root to generate `build/docs/html/index.html` for the
application and BSP.
