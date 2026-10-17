#ifndef SD_SPI_H
#define SD_SPI_H

/**
 * @file sd_spi.h
 * @brief Blocking SD/MMC SPI block driver with a platform callback interface.
 *
 * One global instance supports one card on a dedicated SPI bus. The driver
 * requires no HAL, filesystem, RTOS, or dynamic allocation. All calls, including
 * status and diagnostics, must be serialized by the caller in task/foreground
 * context. Calls are not reentrant and must not be made from an interrupt.
 *
 * Bind an initialized platform port, initialize the card, then perform sector
 * I/O. An operation failure clears readiness. When used with a filesystem,
 * close files and unmount before reinitializing or changing the binding.
 * See README.md for the port contract, integration, and recovery procedure.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Size of one logical sector in bytes, independent of card addressing mode. */
#define SD_SPI_SECTOR_SIZE 512U

/** Result of a card operation; no automatic retry is performed. */
typedef enum {
    SD_OK,              /**< Operation completed successfully. */
    SD_ERROR,           /**< Transfer, protocol, busy timeout, or capacity error. */
    SD_NOT_READY,       /**< Not initialized, or initialization could not complete. */
    SD_INVALID_ARGUMENT /**< NULL argument, zero count, or unrepresentable range. */
} sd_result_t;

/**
 * @brief Cumulative counters and the most recently recorded result.
 * @note Counters start at zero and persist across binding and initialization.
 *       Counters wrap at UINT32_MAX; there is no reset API. One failure may
 *       increment both a callback counter and an operation counter.
 */
typedef struct {
    uint32_t initialization_failures; /**< Unsuccessful initialization calls. */
    uint32_t read_failures;           /**< Failed reads after argument/ready checks. */
    uint32_t write_failures;          /**< Failed writes after argument/ready checks. */
    uint32_t sync_failures;           /**< Failed syncs on an initialized card. */
    uint32_t capacity_failures;       /**< Failed CSD reads or capacity decoding. */
    uint32_t transfer_failures;       /**< Byte-exchange callbacks returning false. */
    uint32_t speed_failures;          /**< Speed callbacks returning false. */
    uint32_t invalid_arguments;       /**< Rejected arguments, including invalid binds. */
    uint32_t not_ready_requests;      /**< Read/write/sync/capacity calls before ready. */
    sd_result_t last_result;          /**< Last operation result or invalid-bind result. */
} sd_spi_diagnostics_t;

/**
 * @brief Required synchronous callbacks for one dedicated SPI bus.
 *
 * Configure mode 0, 8-bit, MSB-first, full-duplex SPI and an inactive CS output
 * before binding. Callbacks execute in the calling context. There is no context
 * pointer; platform state must remain accessible for the entire binding.
 */
typedef struct {
    /**
     * Exchange exactly one byte; store the received byte through rx.
     * Must also clock bytes with CS inactive. Do not change CS. Return false
     * on failure and bound the transfer wait with a platform timeout.
     */
    bool (*exchange)(uint8_t tx, uint8_t *rx);
    /**
     * Set an SPI clock no greater than maximum_hz while CS is inactive.
     * Requests are 400000 Hz for initialization and 4000000 Hz for data I/O.
     * Return false if the limit cannot be met or hardware setup fails.
     */
    bool (*set_speed)(uint32_t maximum_hz);
    /** Assert active-low CS when selected is true; deassert it when false. */
    void (*chip_select)(bool selected);
    /**
     * Return elapsed milliseconds modulo 2^32. Time must advance during
     * polling loops; do not suspend the timebase while the driver is active.
     */
    uint32_t (*get_tick_ms)(void);
    /** Wait at least milliseconds; used for the initial 10 ms settling delay. */
    void (*delay_ms)(uint32_t milliseconds);
} sd_spi_port_t;

/**
 * @brief Copy a platform callback table and clear readiness.
 * @param port Table containing all five callbacks; may have automatic storage.
 * @return true for a complete table; false for NULL or any missing callback.
 *         A rejected binding also removes any previous binding.
 * @note A successful bind deasserts CS without sending card commands. It does
 *       not initialize SPI or the card. Close files, unmount, and leave the
 *       previous card deselected before rebinding. NULL unbinds but returns
 *       false and records SD_INVALID_ARGUMENT. Counters are retained.
 */
bool sd_spi_bind(const sd_spi_port_t *port);

/**
 * @brief Initialize the bound card in SPI mode and set its transfer clock.
 * @retval SD_OK Card is ready; transfer clock is no greater than 4 MHz.
 * @retval SD_NOT_READY No port is bound, a speed request failed, or card
 *         initialization did not complete successfully.
 * @retval SD_ERROR A byte-exchange callback failed.
 * @note Starts with readiness cleared and a clock no greater than 400 kHz.
 *       Every unsuccessful call increments initialization_failures. Close
 *       files and unmount before calling again after a filesystem I/O error.
 */
sd_result_t sd_spi_initialize(void);

/**
 * @brief Return cached readiness without communicating with the card.
 * @return true after successful initialization until rebinding, reinitializing,
 *         or a card operation failure. Invalid arguments do not clear readiness.
 * @note Does not detect removal, check write protection, or update diagnostics.
 */
bool sd_spi_is_ready(void);

/**
 * @brief Read consecutive 512-byte sectors.
 * @param sector First zero-based logical sector, including for SDSC cards.
 * @param buffer Writable storage for count * SD_SPI_SECTOR_SIZE bytes; non-NULL.
 * @param count Positive sector count; its byte size must fit in uint32_t.
 * @retval SD_OK All requested sectors were read.
 * @retval SD_INVALID_ARGUMENT Invalid buffer, count, or address range.
 * @retval SD_NOT_READY The card has not been initialized or lost readiness.
 * @retval SD_ERROR Transfer/protocol failure; clears readiness.
 * @note Validates address arithmetic, not physical capacity. The caller must
 *       keep requests within sd_spi_get_sector_count(). On failure the buffer
 *       may be partially modified; no completed-sector count is returned.
 */
sd_result_t sd_spi_read(uint32_t sector, uint8_t *buffer, uint32_t count);

/**
 * @brief Write consecutive 512-byte sectors and check busy completion/status.
 * @param sector First zero-based logical sector, including for SDSC cards.
 * @param buffer Readable storage for count * SD_SPI_SECTOR_SIZE bytes; non-NULL.
 * @param count Positive sector count; its byte size must fit in uint32_t.
 * @return Same result codes and range rules as sd_spi_read(). SD_ERROR clears
 *         readiness. Some sectors may have been written before an error; no
 *         completed-sector count is returned and the driver does not retry.
 * @note This raw block API does not flush filesystem buffers or metadata.
 */
sd_result_t sd_spi_write(uint32_t sector, const uint8_t *buffer, uint32_t count);

/**
 * @brief Select the card and wait until it releases its busy state.
 * @retval SD_OK Card is no longer busy.
 * @retval SD_NOT_READY Card is not initialized.
 * @retval SD_ERROR Busy timeout or transfer failure; clears readiness.
 * @note Does not flush filesystem or application buffers. Use the filesystem's
 *       sync/close API when a filesystem owns the card.
 */
sd_result_t sd_spi_sync(void);

/**
 * @brief Read and decode the card's CSD capacity in 512-byte sectors.
 * @param count Receives the sector count on success; non-NULL.
 * @retval SD_OK Capacity was decoded and fits in uint32_t.
 * @retval SD_INVALID_ARGUMENT count is NULL.
 * @retval SD_NOT_READY Card is not initialized.
 * @retval SD_ERROR Transfer failure, unsupported CSD, or unrepresentable
 *         capacity; clears readiness. Treat count as valid only on SD_OK.
 */
sd_result_t sd_spi_get_sector_count(uint32_t *count);

/**
 * @brief Copy cumulative counters and the last recorded result.
 * @param diagnostics Output snapshot; NULL is ignored.
 * @note No card access or counter reset. A successful bind and the status and
 *       diagnostics getters do not replace last_result. Serialize this call
 *       with all other driver calls; the copy is not an atomic snapshot.
 */
void sd_spi_get_diagnostics(sd_spi_diagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif
