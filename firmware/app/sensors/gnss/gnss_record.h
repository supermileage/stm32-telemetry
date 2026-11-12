#ifndef GNSS_RECORD_H
#define GNSS_RECORD_H

#include <stdbool.h>
#include <stdint.h>

// Fixed units: udeg = millionths of a degree, mm_s = millimetres per second.
// course_mdeg is direction of travel in thousandths of a degree from true north.
typedef struct {
    bool fix_valid;
    bool speed_valid;
    int32_t latitude_udeg;
    int32_t longitude_udeg;
    int32_t speed_mm_s;
    int32_t course_mdeg;
    bool course_valid;
} gnss_measurement_t;

#endif
