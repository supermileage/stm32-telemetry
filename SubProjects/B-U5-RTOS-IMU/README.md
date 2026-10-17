# IMU experiment

This standalone project runs the onboard ISM330DHCX accelerometer and gyroscope
(U21) on the B-U585I-IOT02A. It uses I2C2 at 7-bit address `0x6B`; STM32 HAL
receives the shifted address `0xD6`. It stays separate from `firmware/` and
its SD card logging system.

## Current behavior

The startup task prints the Git version and project name over UART4, then
suspends instead of deleting itself because the preserved FreeRTOS setup uses
`heap_1`. The IMU task waits 35 ms, checks WHO_AM_I, enables BDU and address
auto-increment, and configures both sensors at 104 Hz (+/-4 g and +/-500 dps).
Initialization failures are reported over UART4 and retried once per second.

The driver drains tagged FIFO words in chunks of at most 32 words, decodes
signed XYZ readings and timestamps, counts overruns and unknown tags, and
captures IMU/MCU clock pairs. Failed I2C reads return an error without decoding
uninitialized data. Each drain is bounded so a continuously filling FIFO
cannot monopolize the task. Latest readings and counters remain in `imu_t`;
UART4 prints the latest gyro (mdps) and accel (mg) readings at most 10 times
per second. Intermediate samples are decoded but are not retained or logged
to SD. Integer UART output does not require printf float support.

The local CubeMX pin and NVIC settings are preserved: PE11 remains a GPIO
input. The task therefore wakes on its 100 ms polling timeout. The incoming
EXTI11 handler, rising-edge callback, and ISR-safe task notification are
retained but inactive with this pin setup. If interrupt operation is enabled
in CubeMX later, its NVIC priority must be numerically at least
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (currently 5).

## Build and check

Import this directory into STM32CubeIDE and build Debug or Release. The local
`makefile.defs` generates `Core/Inc/version.h` from Git; Git must be on PATH.
The existing compiler, linker, HAL, FreeRTOS, clock and peripheral settings
are retained; only `App` source/include paths were added for the driver.
Connect through ST-LINK and use **Run As** or **Debug As > STM32 C/C++ Application**.

The [host suite](../../tests/README.md) exercises FIFO burst boundaries,
signed scaling, timestamp decoding, I2C failures, startup retries' underlying
transactions, and EXTI/task notifications. It does not replace an on-board
check. After flashing this project, inspect UART4 at 115200 baud: verify the
startup banner, changing readings when moving the board, roughly 1000 mg
along the gravity axis when stationary, and recovery from transient I2C errors.
Observe task stack high-water marks and FIFO overrun counters under load.

## Register notes and remaining work

- `CTRL1_XL` and `CTRL2_G` select output data rate and full scale.
- `CTRL3_C` (`0x12`) enables BDU to avoid torn high/low bytes, and `IF_INC` for
  register auto-increment. High-performance operation keeps `XL_HM_MODE` and
  `G_HM_MODE` cleared.
- `CTRL9_XL` enables the recommended `DEVICE_CONF` bit.
- The 3 KB FIFO uses continuous mode (`FIFO_MODE = 110`) to retain newer data
  on overflow. The watermark is 127 words; `INT1_CTRL` routes watermark,
  full, and overrun conditions to INT1. `DIFF_FIFO[9:0]` gives unread words.
- The device automatically wraps FIFO output registers for multiword bursts.
  See the [application note](../../docs/datasheets/imu-ism330dhcs-application-note.pdf)
  and [datasheet](../../docs/datasheets/imu-ism330dhcx-datasheet.pdf).
- Sampling still uses blocking I2C. Gravity removal, mounting calibration,
  bias compensation, and SD integration remain future work. FIFO timestamps
  use the original 32-bit sensor time conversion; long-running timestamp
  rollover handling remains to be implemented before continuous multi-day use.
