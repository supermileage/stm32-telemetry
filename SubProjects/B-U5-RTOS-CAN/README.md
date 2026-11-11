Project to get CAN bus working for BMS, make a CAN interface. Starting with internal loop mode to just test code

CAN transceiver to be used: MCP2562
- Vio to 3.3V/ voltage for data signals to board
- Vdd to 5V to be used for CAN bus signals

- Using MSIS for CAN clock as there is no HSE by default on the board
	- Using auto calibration to get clock precise enough for CAN, should be 0.25% accuracy
- BMS talks CAN V2 so classic mode is used
- Use a maximum unterminated stub length of 0.3 m
- Max bus length of 30-40m to reach 1Mb/s
- Our wiring isn't very long so it should be easy to reach the maximum 1Mb/s. A standard 20 quanta nominal bit time should be good. This will be split as 15 for seg 1 and 4 for seg 2, and an implicit 1 for sync. This also follows recommendation of sampling in the later 20% for more tolerance to being out of sync. To reach 20 quanta nominal bit time a prescaler of 8 is used. A sync jump width of 4 allows for a clock tolerance of up to 0.78%
	- The other CAN devices run on 500Kb/s which should be matched. Prescaler changed to 16
- Put a pullup on RX to default to recessive to guard against noise when disconnected from bus
