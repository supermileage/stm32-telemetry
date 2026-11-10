#include "app.h"
#include "main.h"
#include "usart.h"
#include "logging/logger.h"
#include "sensors/gnss_task.h"

void app_init(void)
{
    logger_start_task();
    gnss_start_task(&huart3);
}
