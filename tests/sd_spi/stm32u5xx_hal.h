#ifndef SD_SPI_TEST_HAL_H
#define SD_SPI_TEST_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int id; } SPI_TypeDef;
typedef struct { int id; } GPIO_TypeDef;
extern SPI_TypeDef mock_spi1, mock_spi2, mock_spi3;
#define SPI1 (&mock_spi1)
#define SPI2 (&mock_spi2)
#define SPI3 (&mock_spi3)

typedef struct {
    uint32_t Mode, Direction, DataSize, FirstBit, CLKPolarity, CLKPhase;
    uint32_t NSS, TIMode, CRCCalculation, BaudRatePrescaler, NSSPMode;
    uint32_t MasterKeepIOState;
} SPI_InitTypeDef;
typedef enum { HAL_SPI_STATE_RESET, HAL_SPI_STATE_READY, HAL_SPI_STATE_BUSY } HAL_SPI_StateTypeDef;
typedef struct {
    SPI_TypeDef *Instance;
    SPI_InitTypeDef Init;
    HAL_SPI_StateTypeDef State;
} SPI_HandleTypeDef;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;

#define SPI_MODE_MASTER 1U
#define SPI_DIRECTION_2LINES 2U
#define SPI_DATASIZE_8BIT 8U
#define SPI_FIRSTBIT_MSB 0U
#define SPI_POLARITY_LOW 0U
#define SPI_PHASE_1EDGE 0U
#define SPI_NSS_SOFT 1U
#define SPI_TIMODE_DISABLE 0U
#define SPI_CRCCALCULATION_DISABLE 0U
#define SPI_NSS_PULSE_DISABLE 0U
#define SPI_MASTER_KEEP_IO_STATE_ENABLE 1U
#define SPI_BAUDRATEPRESCALER_2 2U
#define SPI_BAUDRATEPRESCALER_4 4U
#define SPI_BAUDRATEPRESCALER_8 8U
#define SPI_BAUDRATEPRESCALER_16 16U
#define SPI_BAUDRATEPRESCALER_32 32U
#define SPI_BAUDRATEPRESCALER_64 64U
#define SPI_BAUDRATEPRESCALER_128 128U
#define SPI_BAUDRATEPRESCALER_256 256U
#define RCC_PERIPHCLK_SPI1 1ULL
#define RCC_PERIPHCLK_SPI2 2ULL
#define RCC_PERIPHCLK_SPI3 3ULL

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,
    const uint8_t *tx, uint8_t *rx, uint16_t count, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *spi);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t delay);
uint32_t HAL_RCCEx_GetPeriphCLKFreq(uint64_t clock);

#ifdef __cplusplus
}
#endif

#endif
