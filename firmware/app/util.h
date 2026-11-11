/*
 * util.h
 *
 *  Created on: Nov 7, 2026
 *      Author: Faysal
 */

#ifndef UTIL_H_
#define UTIL_H_

//return early if HAL error, only use on functions with return type void
#define HAL_TRY(expr) do{ \
		HAL_StatusTypeDef status = (expr); \
		if (status != HAL_OK) return; \
	} while(0)

static inline uint16_t min_u16(uint16_t a, uint16_t b) {
	return (a < b) ? a : b;
}

static inline uint16_t combine_u8_to_u16(uint8_t a, uint8_t b) {
	return ((uint16_t) a << 8) | b;
}

static inline uint32_t combine_u8_to_u32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return ((uint32_t) a << 24) | ((uint32_t) b << 16) | ((uint32_t) c << 8) | d;
}

#endif /* UTIL_H_ */
