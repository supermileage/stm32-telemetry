/**
 * @file
 * @brief SD interface setup for the board's SPI1 bus and PE12 chip select.
 */
#include "board_sd.h"
#include "sd_spi_port.h"
#include "main.h"
#include "spi.h"

namespace bsp {

bool configureSd() noexcept
{
    return sd_spi_port_bind(&hspi1, SD_CS_GPIO_Port, SD_CS_Pin);
}

} // namespace bsp

bool board_sd_configure(void)
{
    return bsp::configureSd();
}
