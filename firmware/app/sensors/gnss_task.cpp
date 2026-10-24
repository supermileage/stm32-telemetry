/**
 * @file
 * @brief Receives GNSS UART data and assembles checksum-validated NMEA sentences.
 */
#include "gnss_task.h"
#include "minmea.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#include <cstring>


