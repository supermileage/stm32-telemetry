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
    /** FatFs handle for the current file. */
    FIL file;
    /** Bytes waiting to be written. */
    uint8_t buffer[LOG_BUFFER_SIZE];
    /** Number of valid bytes in buffer. */
    UINT used;
    /** File size including bytes still buffered. */
    uint32_t size;
    /** Next LOGxxxxx.BIN number to try. */
    uint32_t next_number;
    /** Saved error; later writes stop after a failure. */
    FRESULT error;
    /** Whether the FatFs handle needs closing. */
    bool open;
    /** Current or most recently attempted filename. */
    char path[LOG_PATH_SIZE];
} log_file_t;

/**
 * @brief Open the first unused LOGxxxxx.BIN and sync its header.
 * Requires mounted storage. Call log_file_close() after failure before unmounting.
 * @param[in,out] log Closed, zero-initialized writer.
 * @return FR_OK, FR_EXIST if names are exhausted, or a FatFs error.
 */
FRESULT log_file_open(log_file_t *log);

/**
 * @brief Buffer a framed record, rotating at LOG_ROTATE_BYTES.
 * Success does not mean synced. Do not retry failed appends; writes may be partial.
 * @param[in,out] log Open writer owned by the storage task.
 * @param record Sample with length at most LOG_PAYLOAD_MAX.
 * @return FR_OK if buffered, otherwise a parameter, state, or I/O error.
 */
FRESULT log_file_append(log_file_t *log, const log_record_t *record);

/**
 * @brief Write the buffer and sync file data and metadata.
 * @param[in,out] log Open writer owned by the storage task.
 * @return FR_OK on success, otherwise a parameter, state, or I/O error.
 */
FRESULT log_file_sync(log_file_t *log);

/**
 * @brief Flush and close, attempting cleanup even after a write failure.
 * @param[in,out] log Writer to close; already closed writers return their saved error.
 * @return The first saved, write, or close error, or FR_OK.
 */
FRESULT log_file_close(log_file_t *log);

#endif
