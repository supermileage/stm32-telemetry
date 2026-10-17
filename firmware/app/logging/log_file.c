/**
 * @file
 * @brief Binary framing, buffered writes, and log file rotation.
 */
#include "log_file.h"

#include <stdio.h>
#include <string.h>

static void put_u16_le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32_le(uint8_t *p, uint32_t value)
{
    for (unsigned int i = 0U; i < 4U; ++i) {
        p[i] = (uint8_t)(value >> (8U * i));
    }
}

/* Reflected CRC-32/ISO-HDLC, matching the on-disk format. */
static uint32_t crc32(const uint8_t *p, UINT size)
{
    uint32_t crc = UINT32_MAX;
    for (UINT i = 0U; i < size; ++i) {
        crc ^= p[i];
        for (unsigned int bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xEDB88320UL);
        }
    }
    return ~crc;
}

static FRESULT flush_buffer(log_file_t *log)
{
    if (log->error != FR_OK) {
        return log->error;
    }
    if (log->used == 0U) {
        return FR_OK;
    }
    UINT written = 0U;
    log->error = f_write(&log->file, log->buffer, log->used, &written);
    /* FatFs reports disk full as FR_OK with fewer bytes written. */
    if (log->error == FR_OK && written != log->used) {
        log->error = FR_DENIED;
    }
    if (log->error == FR_OK) {
        log->used = 0U;
    }
    return log->error;
}

static FRESULT open_next(log_file_t *log)
{
    FRESULT result = FR_EXIST;
    while (log->next_number < 100000U) {
        (void)snprintf(log->path, sizeof(log->path), "0:/LOG%05lu.BIN",
            (unsigned long)log->next_number++);
        result = f_open(&log->file, log->path, FA_WRITE | FA_CREATE_NEW);
        if (result != FR_EXIST) {
            break;
        }
    }
    log->error = result;
    if (result != FR_OK) {
        return result;
    }
    log->open = true;
    /* File header: magic, version, header size, timestamp Hz, reserved. */
    static const uint8_t header[16] = {
        'S', 'T', 'M', 'L', 1, 0, 16, 0, 0xE8, 3, 0, 0, 0, 0, 0, 0
    };
    memcpy(log->buffer, header, sizeof(header));
    log->used = sizeof(header);
    log->size = sizeof(header);
    return log_file_sync(log);
}

FRESULT log_file_open(log_file_t *log)
{
    if (log == NULL) {
        return FR_INVALID_PARAMETER;
    }
    if (log->open) {
        return FR_INVALID_OBJECT;
    }
    memset(log, 0, sizeof(*log));
    return open_next(log);
}

FRESULT log_file_append(log_file_t *log, const log_record_t *record)
{
    if (log == NULL || record == NULL || record->length > LOG_PAYLOAD_MAX) {
        return FR_INVALID_PARAMETER;
    }
    if (!log->open) {
        return FR_INVALID_OBJECT;
    }
    if (log->error != FR_OK) {
        return log->error;
    }
    UINT size = 16U + record->length;
    if (log->size + size > LOG_ROTATE_BYTES) {
        FRESULT result = log_file_close(log);
        if (result != FR_OK) {
            return result;
        }
        result = open_next(log);
        if (result != FR_OK) {
            return result;
        }
    }
    if (log->used + size > LOG_BUFFER_SIZE && flush_buffer(log) != FR_OK) {
        return log->error;
    }
    uint8_t *frame = log->buffer + log->used;
    memcpy(frame, "REC1", 4U);
    put_u32_le(frame + 4U, record->timestamp_ms);
    put_u16_le(frame + 8U, record->source);
    put_u16_le(frame + 10U, record->length);
    memcpy(frame + 12U, record->payload, record->length);
    put_u32_le(frame + 12U + record->length, crc32(frame, 12U + record->length));
    log->used += size;
    log->size += size;
    return FR_OK;
}

FRESULT log_file_sync(log_file_t *log)
{
    if (log == NULL) {
        return FR_INVALID_PARAMETER;
    }
    if (!log->open) {
        return FR_INVALID_OBJECT;
    }
    if (flush_buffer(log) == FR_OK) {
        log->error = f_sync(&log->file);
    }
    return log->error;
}

FRESULT log_file_close(log_file_t *log)
{
    if (log == NULL) {
        return FR_INVALID_PARAMETER;
    }
    if (!log->open) {
        return log->error;
    }
    FRESULT result = flush_buffer(log);
    FRESULT closed = f_close(&log->file);
    log->open = false;
    log->error = result == FR_OK ? closed : result;
    return log->error;
}
