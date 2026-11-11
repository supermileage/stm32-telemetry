/** GPS measurements shared by the parser and logger. */
#ifndef GNSS_RECORD_H
#define GNSS_RECORD_H

#include <stdbool.h>
#include <stdint.h>

/** Divide value by scale to read the number. A scale of zero or less means missing. */
typedef struct {
    int32_t value;
    int32_t scale;
} gnss_fraction_t;

/** Position and speed from one RMC sentence. */
typedef struct {
    bool fix_valid;

    /** Signed NMEA degrees and minutes. The CSV formatter converts to degrees. */
    gnss_fraction_t latitude;
    gnss_fraction_t longitude;

    /** Speed over ground in knots. */
    gnss_fraction_t speed;
} gnss_measurement_t;

#endif
