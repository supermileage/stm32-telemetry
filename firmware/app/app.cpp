#include "app.h"
#include "main.h"
#include "usart.h"
#include "logging/logger.h"
#include "sensors/gnss/gnss_task.h"
#include "sensors/fcc/fcc_task.hpp"
#include "sensors/fcc/fcc_protocol.hpp"
#include "sensors/imu_task.h"
#include "sensors/flowmeter_task.h"

void app_init(void) {
    logger_start_task();
    gnss_start_task(&huart3);
    if (!fcc_start_task(&hlpuart1, fcc_protocol::PacketGapMs)) {
        Error_Handler();
    }
    IMU_start_task();
    FlowMeter_start_task();
}
