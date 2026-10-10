#ifndef SD_SPI_H
#define SD_SPI_H

/**
 * @file sd_spi.h
 * @brief SD card block I/O for a dedicated SPI bus.
 *
 * The storage task owns the card and all calls to this API. An I/O failure
 * clears the ready state; close files and unmount before initializing again.
 */

#include <stdbool.h>
#include <stdint.h>

#define SD_SPI_SECTOR_SIZE 512U

typedef enum {
    SD_OK,
    SD_ERROR,
    SD_NOT_READY,
    SD_INVALID_ARGUMENT
} sd_result_t;

typedef struct {
    uint32_t initialization_failures;
    uint32_t read_failures;
    uint32_t write_failures;
    uint32_t sync_failures;
    uint32_t capacity_failures;
    uint32_t transfer_failures;
    uint32_t speed_failures;
    uint32_t invalid_arguments;
    uint32_t not_ready_requests;
    sd_result_t last_result;
} sd_spi_diagnostics_t;

/** Hardware callbacks for one dedicated bus, called only by the storage task. */
typedef struct {
    bool (*exchange)(uint8_t tx, uint8_t *rx); /**< One byte; false on transfer failure. */
    bool (*set_speed)(uint32_t maximum_hz);   /**< Choose a clock at or below this limit. */
    void (*chip_select)(bool selected);      /**< Assert CS when true. */
    uint32_t (*get_tick_ms)(void);           /**< Monotonic milliseconds, wraps at UINT32_MAX. */
    void (*delay_ms)(uint32_t milliseconds);
} sd_spi_port_t;

/**
 * @brief Bind hardware callbacks and clear readiness. The callbacks are copied.
 * @return false if any callback is missing. NULL unbinds and returns false.
 * @note Close files and unmount before rebinding. All calls use task context.
 */
bool sd_spi_bind(const sd_spi_port_t *port);

/** @brief Enter SPI mode at a slow clock, then switch to the transfer clock. */
sd_result_t sd_spi_initialize(void);

/** @brief Return cached readiness. There is no physical card-detect input. */
bool sd_spi_is_ready(void);

/**
 * @brief Read consecutive 512-byte sectors.
 * @param sector First logical sector (block address, including for SDSC cards).
 * @param buffer Destination with room for count * SD_SPI_SECTOR_SIZE bytes.
 * @param count Number of sectors; must be greater than zero.
 * @return SD_OK on success. SD_ERROR invalidates the card until reinitialized.
 */
sd_result_t sd_spi_read(uint32_t sector, uint8_t *buffer, uint32_t count);

/** @brief Write consecutive sectors and wait for the card's write status. */
sd_result_t sd_spi_write(uint32_t sector, const uint8_t *buffer, uint32_t count);

/** @brief Wait for the card to finish any internal write operation. */
sd_result_t sd_spi_sync(void);

/** @brief Read the card's CSD capacity in 512-byte sectors. */
sd_result_t sd_spi_get_sector_count(uint32_t *count);

/** @brief Copy lifetime error counters. Call from the storage task. */
void sd_spi_get_diagnostics(sd_spi_diagnostics_t *diagnostics);

#endif
