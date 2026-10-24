/**
 * @file
 * @brief Application setup called before the RTOS scheduler starts.
 */
#include "app.h"
/* Prevent C++ name mangling */
extern "C" {
#include "storage/storage_task.h"
}

extern "C" void app_init(void)
{
    storage_start_task();
}
