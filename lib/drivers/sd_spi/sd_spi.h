#ifndef SD_SPI_H
#define SD_SPI_H

/**
 * @file sd_spi.h
 * @brief Blocking access to one SD card over SPI.
 *
 * Set up mode-0 SPI with a fixed clock of 400 kHz or lower before binding.
 * The driver leaves the peripheral settings alone and uses no HAL, RTOS,
 * filesystem, or dynamic allocation.
 *
 * Keep all calls in one task or foreground context. Don't call the driver
 * from interrupts or from two places at once. After an I/O error, close any
 * files and unmount before rebinding or initializing the card again.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Each logical sector holds 512 bytes, including on byte-addressed cards. */
#define SD_SPI_SECTOR_SIZE 512U

/** Result of a card operation. The driver does not retry failed operations. */
typedef enum {
    /** The operation finished successfully. */
    SD_OK,
    /** A transfer, card command, timeout, or capacity check failed. */
    SD_ERROR,
    /** The card is not initialized or initialization failed. */
    SD_NOT_READY,
    /** A pointer, count, or address range is invalid. */
    SD_INVALID_ARGUMENT
} sd_result_t;

/**
 * @brief Error counts and the most recent operation result.
 *
 * Counters start at zero, wrap at 32 bits, and survive rebinding and
 * initialization. One failure can count against both a transfer and the
 * operation that used it. Invalid arguments and requests made before the card
 * is ready have their own counters.
 */
typedef struct {
    /** Number of failed attempts to initialize the card. */
    uint32_t initialization_failures;
    /** Reads that failed after the card was ready. */
    uint32_t read_failures;
    /** Writes that failed after the card was ready. */
    uint32_t write_failures;
    /** Sync operations that failed after the card was ready. */
    uint32_t sync_failures;
    /** Attempts to read or decode the card's capacity that failed. */
    uint32_t capacity_failures;
    /** Times the byte-transfer callback returned false. */
    uint32_t transfer_failures;
    /** Calls rejected because of invalid arguments, including bind calls. */
    uint32_t invalid_arguments;
    /** Read, write, sync, or capacity requests made while the card was not ready. */
    uint32_t not_ready_requests;
    /** Most recent operation result, including a rejected bind. */
    sd_result_t last_result;
} sd_spi_diagnostics_t;

/**
 * @brief Hardware callbacks for one dedicated SPI bus.
 *
 * Configure full-duplex, 8-bit, MSB-first SPI and leave CS high before binding.
 * Each callback runs in the context of the caller. There is no context pointer,
 * so the hardware state used by these callbacks must stay valid while bound.
 */
typedef struct {
    /**
     * @brief Send and receive one byte without changing CS.
     *
     * This must also work with CS high, since SD startup needs dummy clocks.
     * Use a finite transfer timeout.
     *
     * @param tx Byte to send.
     * @param[out] rx Where to put the received byte.
     * @return true if the transfer succeeded; false on a hardware error or timeout.
     */
    bool (*exchange)(uint8_t tx, uint8_t *rx);
    /**
     * @brief Set the card's active-low chip-select output.
     * @param selected true to pull CS low; false to raise it.
     */
    void (*chip_select)(bool selected);
    /**
     * @brief Read the millisecond tick counter.
     *
     * It must keep advancing while the driver polls, and wrap at 32 bits.
     *
     * @return Current tick count in milliseconds.
     */
    uint32_t (*get_tick_ms)(void);
    /**
     * @brief Wait for at least the requested number of milliseconds.
     * @param milliseconds Delay length; startup requests 10 ms.
     */
    void (*delay_ms)(uint32_t milliseconds);
} sd_spi_port_t;

/**
 * @brief Copy the hardware callbacks and clear readiness.
 * Raises CS without initializing hardware. Close files, unmount, and deselect the
 * old card before rebinding. Diagnostic counters are kept.
 * @param port Complete callback table; copied, so it may be local. NULL clears the binding.
 * @return true if bound; false for NULL or missing callbacks, clearing any old binding.
 */
bool sd_spi_bind(const sd_spi_port_t *port);

/**
 * @brief Initialize the card at the preconfigured clock of 400 kHz or lower.
 * Failed attempts clear readiness and increment initialization_failures.
 * @retval SD_OK The card is ready.
 * @retval SD_NOT_READY No port is bound or card initialization failed.
 * @retval SD_ERROR A byte transfer failed.
 */
sd_result_t sd_spi_initialize(void);

/**
 * @brief Read the saved ready flag without checking the physical card.
 * @return true after initialization until rebinding, reinitializing, or an I/O failure.
 *         Invalid arguments do not clear readiness.
 */
bool sd_spi_is_ready(void);

/**
 * @brief Read consecutive 512-byte sectors.
 * Check the card capacity first. Failed reads may partially change the buffer.
 * @param sector First zero-based sector, including for SDSC cards.
 * @param[out] buffer Space for count * SD_SPI_SECTOR_SIZE bytes.
 * @param count Positive sector count; the byte count must fit in uint32_t.
 * @return SD_OK if complete, otherwise an sd_result_t error. SD_ERROR clears readiness.
 */
sd_result_t sd_spi_read(uint32_t sector, uint8_t *buffer, uint32_t count);

/**
 * @brief Write consecutive 512-byte sectors and wait for completion.
 * Check capacity first. Failed writes may be partial; no retries or completed-sector
 * count are provided. This does not flush application or filesystem buffers.
 * @param sector First zero-based sector, including for SDSC cards.
 * @param buffer Data covering count * SD_SPI_SECTOR_SIZE bytes.
 * @param count Positive sector count; the byte count must fit in uint32_t.
 * @return SD_OK if complete, otherwise an sd_result_t error. SD_ERROR clears readiness.
 */
sd_result_t sd_spi_write(uint32_t sector, const uint8_t *buffer, uint32_t count);

/**
 * @brief Wait until the card is no longer busy.
 * Use filesystem sync/close to flush filesystem buffers.
 * @retval SD_OK The card is no longer busy.
 * @retval SD_NOT_READY The card is not initialized.
 * @retval SD_ERROR A timeout or transfer failed; readiness is cleared.
 */
sd_result_t sd_spi_sync(void);

/**
 * @brief Read capacity in 512-byte sectors.
 * @param[out] count Non-NULL output; valid only after SD_OK.
 * @return SD_OK, SD_INVALID_ARGUMENT for NULL, SD_NOT_READY before initialization,
 *         or SD_ERROR if capacity cannot be read or represented in uint32_t.
 */
sd_result_t sd_spi_get_sector_count(uint32_t *count);

/**
 * @brief Copy counters and last_result without accessing the card or resetting them.
 * Call from the same context as other driver operations; no lock is taken.
 * @param[out] diagnostics Snapshot destination; NULL is ignored.
 */
void sd_spi_get_diagnostics(sd_spi_diagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif
