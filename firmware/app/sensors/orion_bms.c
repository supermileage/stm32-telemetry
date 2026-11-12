/*
 * orion_bms.c
 *
 *  Created on: Nov 11, 2026
 *      Author: Faysal
 */

#include <stdint.h>

// Faults are prioritized from left to right, most significant bit to least significant
// and numbered from 1 (highest) to 19 (lowest).  Please refer to notion for more info
uint8_t parseFault(uint8_t data[]) {
	uint8_t fault = 1;
	for (int i = 1; i < 4; i++) {
		uint8_t currentByte = data[i];

		int bitCount = 8;
		if (i == 3)
			bitCount = 3;

		for (int j = 0; j < bitCount; j++) {
			if (currentByte & (1 << (bitCount - j - 1))) {
				return fault > 5 ? fault + 2 : fault;
			}
			fault++;
		}
	}
	return 0;
}
