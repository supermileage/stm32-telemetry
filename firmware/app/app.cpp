/**
 * @file
 * @brief Application setup called before the RTOS scheduler starts.
 */
#include "app.h"
#include "board_sd.h"
#include "main.h"
/* Prevent C++ name mangling */
extern "C" {
#include "storage/storage_task.h"
}

extern "C" void app_init(void)
{
    if (!bsp::configureSd()) {
        Error_Handler();
    }

    //storage_start_hello_test();
    storage_start_task();

}
