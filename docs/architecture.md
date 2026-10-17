# Firmware architecture

The STM32CubeIDE project is in `firmware/`. Application code stays outside
generated files so CubeMX can regenerate setup without replacing it.

## Where code belongs

| Directory | Responsibility |
| --- | --- |
| `firmware/Core` | Generated peripheral, interrupt, and RTOS setup |
| `firmware/Drivers` | STM32 HAL and CMSIS |
| `firmware/Middlewares` | FreeRTOS and CMSIS-RTOS integration |
| `firmware/app` | Application startup and sensor tasks |
| `firmware/app/logging` | Record queue and binary log writer |
| `firmware/app/storage` | Storage task, mounting, and file check |
| `firmware/BSP` | Board pins and HAL adapters |
| `lib/drivers/sd_spi` | Portable SD protocol and block I/O |
| `lib/drivers/sd_spi/fatfs` | FatFs disk adapter |
| `lib/fatfs` | Pinned FatFs sources, configuration, and CubeIDE wrappers |
