# Board experiments

These projects test individual pieces before they move into the main firmware.
Import each project directory into STM32CubeIDE and build or debug it through
the board's ST-LINK connection.

- [B-U5-RTOS-Blink](B-U5-RTOS-Blink/README.md): blink the green LED from a FreeRTOS task.
- [B-U5-RTOS-IMU](B-U5-RTOS-IMU/README.md): sample the onboard IMU through its tagged FIFO over I2C, independently of SD logging.

Name new projects `<board>-<environment>-<description>`. `B-U5` means the
B-U585I-IOT02A board; use a short description such as a sensor name. Keep
prototype code here until it is ready to join the main application.
