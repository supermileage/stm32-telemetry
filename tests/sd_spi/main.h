#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H
#include "stm32u5xx_hal.h"
extern GPIO_TypeDef gpio;
#define SD_CS_GPIO_Port (&gpio)
#define SD_CS_Pin (1U << 12)
#endif
