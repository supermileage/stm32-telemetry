#include "app.h"
#include "storage/storage_task.h"

void app_init(void)
{
    storage_start_task();
}
