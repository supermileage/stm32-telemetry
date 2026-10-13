## Notes

The board has an in built 3D accelerometer and gyroscope, ISM330DHCX. 
- It is connected to the second I2C bus, I2C2.
- The 7 bit address is 1101011
- After the start condition (ST) a slave address is sent, once a slave acknowledge (SAK) has been
- returned, an 8-bit sub-address (SUB) is transmitted. The increment of the address is configured by the CTRL3_C (12h) (IF_INC). (default increments after each access)
- takes 35ms to turn on (table 3)
- ODR = output data rate
- ODR_XL[3:0] in CTRL1_XL controls sampling rate or off of accelerometer. View table 43 in datasheet for bit to sampling rate selections. To use high performance bit XL_HM_MODE bit in CTRL6_C should be cleared, default 0.
- ODR_G[3:0] in CTRL2_G controls sampling rate or off of gyroscope. View table 46 in datasheet for bit to sampling rate selections. To use high performance bit G_HM_MODE bit in CTRL7_G should be cleared, default 0.
- FS = full scale = maximum measurement it can measure
	- The amount of steps is always the same but the resolution is what changes. Using 2g gives us a resolution 0.061mg per step, using 16g 0.488mg.
	- DPS = degreees per second
- DEVICE_CONF bit in CTRL9_XL is recommended to set, it starts at 0
- I believe BDU can help us make sure low and high byte of reading stay in sync. It freezes high and low byte while reading so one doesn't change while reading and we get a mismatch.
- Consider circular burst mode (rounding)
- vTaskDelete cannot be used with heap_1 as it cannot free the task from the heap and will just stall
- The IMU has designator U21 on the board
- To account for gravity for the accelerometer a calibration step is needed as it depends on how the IMU is titled when the car is on flat ground. We take those values and subtract them for all future readings.
- Accelerometer is at most +-65mg, typical 10
- Gyroscope is at most +- 3000 mdps, typical 1000
- FIFO
	- The IMU has a 3kB FIFO (9kB with compression enabled) which can automatically take in sensor data
	- A sensible approach to not poll too much and to not miss data would be to allow the sensor data to fill in the FIFO. A low priority task can flush it when nothing higher priority needs to run. An interrupt can be set to trigger when the FIFO is a certain level full this would force the us to read the FIFO right away. This ensures the FIFO doesn't get full/we miss data and we minimize the amount of interrupts by doing it when we are free.
	- Status flags are set when certain things happen such as FIFO full or past a threshold or started missing data. It would make sense to check these on a read to log them if we do miss data and reset them to know if it happens again.
	- Continuous FIFO mode (FIFO_MODE[2:0] = 110 in FIFO_CTRL4) is used to overwrite old data with new if the FIFO is full. The driver needs the current speed not old which is why this mode is picked
	- The FIFO watermark sets the amount unread samples that can accumulate before FIFO_WTM_IA (in FIFO_STATUS2) is set
		- To throw an interrupt when this happens bit INT1_FIFO_TH in INT1_CTRL should be set
	- When old data has become overwritten then FIFO_OVR_IA bit in FIFO_STATUS2 is set
	- Get the number of unread samples to empty FIFO before it's full from DIFF_FIFO_[9:0] in FIFO_STATUS1&2


## TODO
- [ ] Use non blocking I2C reads
- [ ] Check for errors from I2C calls
