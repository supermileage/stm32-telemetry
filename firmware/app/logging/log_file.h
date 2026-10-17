/**
 * @file
 * @brief Buffered binary log writer, used only by the storage task.
 */
#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "ff.h"
#include "record.h"

#include <stdbool.h>

/** Size of the RAM write buffer in bytes. */
#define LOG_BUFFER_SIZE 4096U
/** Maximum file size, including the header and record framing. */
#define LOG_ROTATE_BYTES (64UL * 1024UL * 1024UL)
/** Capacity of a log path, including its NUL terminator. */
#define LOG_PATH_SIZE 17U

/**
 * Writer state owned by the storage task; no internal locking is provided.
 * Keep it in static storage because the buffer is too large for the task stack.
 * Zero-initialize before the first open and leave the fields to the writer.
 */
typedef struct {
    FIL file;                        /**< FatFs handle for the current file. */
    uint8_t buffer[LOG_BUFFER_SIZE];  /**< Bytes waiting to be written. */
    UINT used;                       /**< Number of valid bytes in buffer. */
    uint32_t size;                    /**< File size including bytes still buffered. */
    uint32_t next_number;             /**< Next LOGxxxxx.BIN number to try. */
    FRESULT error;                    /**< Saved error; later writes stop after a failure. */
    bool open;                       /**< Whether the FatFs handle needs closing. */
    char path[LOG_PATH_SIZE];         /**< Current or most recently attempted filename. */
} log_file_t;

/**
 * Create the first unused LOGxxxxx.BIN, starting at LOG00000.BIN, and sync its header.
 * Requires mounted storage. On failure, call log_file_close() before unmounting:
 * a failed header sync can leave the file open.
 * @param[in,out] log Writer, zero-initialized before first use and currently closed.
 * @return FR_OK on success, FR_EXIST if all names are used, or a FatFs error.
 */
FRESULT log_file_open(log_file_t *log);

/**
 * Buffer one framed record, rotating before the file would exceed LOG_ROTATE_BYTES.
 * A full buffer is written as needed; success does not guarantee a completed sync.
 * Stop on errors rather than retrying a record that may have been partly written.
 * @param[in,out] log Open writer owned by the calling storage task.
 * @param record Sample to encode, with length at most LOG_PAYLOAD_MAX.
 * @return FR_OK if buffered, or a parameter, state, or FatFs error.
 */
FRESULT log_file_append(log_file_t *log, const log_record_t *record);

/**
 * Write buffered records and sync the file's data and metadata.
 * @param[in,out] log Open writer owned by the calling storage task.
 * @return FR_OK after a successful sync, or a parameter, state, or saved I/O error.
 */
FRESULT log_file_sync(log_file_t *log);

/**
 * Flush and close the file, attempting the close even after a write failure.
 * @param[in,out] log Writer to close; an already closed writer returns its saved error.
 * @return The first saved, write, or close error, or FR_OK on success.
 */
FRESULT log_file_close(log_file_t *log);

#endif
