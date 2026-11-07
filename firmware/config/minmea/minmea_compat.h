#pragma once

/* Newlib does not provide timegm(). This firmware runs with UTC as its
 * C runtime timezone, so minmea can safely use mktime() as a fallback.
 * Do not change the timezone to local time.
 */
#include <time.h>
#define timegm mktime
