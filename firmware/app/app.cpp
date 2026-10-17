/**
 * @file
 * @brief Application setup called before the RTOS scheduler starts.
 */
#include "app.h"
#include "board_sd.h"
#include "main.h"

static_assert(__cplusplus >= 201703L, "Application code requires C++17");

extern "C" void app_init(void)
{
    if (!bsp::configureSd()) {
        Error_Handler();
    }
}
