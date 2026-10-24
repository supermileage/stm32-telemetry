#include "board_sd.h"
#include "storage/sd_spi_port.hpp"
#include "main.h"
#include "spi.h"

bool board_sd_configure(void)
{
    static spi_bus_t bus{};

    if (!spi_bus_init(&bus, &hspi1)) {
        (void)sd_spi_port_bind(nullptr, nullptr, 0U);
        return false;
    }
    return sd_spi_port_bind(&bus, SD_CS_GPIO_Port, SD_CS_Pin);
}
