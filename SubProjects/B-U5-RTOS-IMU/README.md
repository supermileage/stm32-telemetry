# IMU experiment

This project begins configuring the onboard ISM330DHCX accelerometer and
gyroscope on the B-U585I-IOT02A. It uses I2C2 at 7-bit address `0x6B`; the
STM32 HAL address argument is that value shifted left by one bit (`0xD6`).

The startup task prints the firmware version and project name over UART4.
The IMU task waits 35 ms, then writes `0x80` to `CTRL1_XL` (`0x10`) to select
the accelerometer's 1.66 kHz output data rate with the other fields left at
zero. Reading samples and printing them are still TODOs; the task currently
waits in a loop after configuration. It does not configure the gyroscope.

Import this directory into STM32CubeIDE and build it. The local
`makefile.defs` generates `Core/Inc/version.h` from Git; Git must be on PATH.
Connect through ST-LINK and use **Run As** or **Debug As > STM32 C/C++ Application**.

Useful register notes for the next step:

- `CTRL1_XL` sets the accelerometer output data rate (`ODR_XL`); see table 43
  in the [datasheet](../../docs/datasheets/imu-ism330dhcx-datasheet.pdf).
- `CTRL2_G` sets the gyroscope output data rate (`ODR_G`); see table 46.
- `CTRL3_C` (`0x12`) controls register-address auto-increment through `IF_INC`.
- High-performance operation uses `XL_HM_MODE = 0` in `CTRL6_C` and
  `G_HM_MODE = 0` in `CTRL7_G`.

The I2C write result is currently unchecked. This is prototype code, not yet
a sensor driver used by the main firmware.
