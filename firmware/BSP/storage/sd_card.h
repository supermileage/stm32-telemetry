#ifndef SD_CARD_H
#define SD_CARD_H

// SD card setup on SPI1.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Connect the SD driver to SPI1.
// Logger task only, with files closed and the card unmounted.
// Requires mode 0 at 400 kHz or less. FatFs initializes the card later.
// Returns true if configured.
bool sd_card_configure(void);

#ifdef __cplusplus
}
#endif

#endif
