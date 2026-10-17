#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H
#include "stm32u5xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif
extern GPIO_TypeDef gpio;
#ifdef __cplusplus
}
#endif
#define SD_CS_GPIO_Port (&gpio)
#define SD_CS_Pin (1U << 12)
#endif
