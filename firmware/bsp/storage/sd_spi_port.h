#pragma once
#include "SD_reader.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SD SPI access for the FatFs adapter.
// Logger task only. Configure before use and pair acquire with release.

// Lock SPI1 with CS high for one SD operation.
// Returns the card handle until release, or NULL if the lock fails.
SD_Card *sd_spi_port_acquire(void);

// Deselect the card and unlock SPI1.
// I/O errors require card initialization before reuse.
// Returns true if the operation completed without callback errors.
bool sd_spi_port_release(void);

// Check cached card status without reading hardware.
// Returns true if initialized with no recorded I/O failure.
bool sd_spi_port_is_ready(void);

#ifdef __cplusplus
}
#endif
