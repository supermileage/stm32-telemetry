#include "log_file.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t output[8192];
static UINT output_size;
static unsigned int writes, syncs, closes, collisions;
static bool short_write;
static FRESULT write_error, sync_error, close_error;
static char opened_path[17];

FRESULT f_open(FIL *file, const TCHAR *path, BYTE mode)
{
    (void)file;
    assert(mode == (FA_WRITE | FA_CREATE_NEW));
    if (collisions != 0U) { --collisions; return FR_EXIST; }
    strcpy(opened_path, path);
    output_size = 0;
    return FR_OK;
}
FRESULT f_write(FIL *file, const void *data, UINT count, UINT *written)
{
    (void)file;
    ++writes;
    *written = short_write ? count / 2U : count;
    if (write_error != FR_OK) *written = 0;
    assert(output_size + *written <= sizeof(output));
    memcpy(output + output_size, data, *written);
    output_size += *written;
    return write_error;
}
FRESULT f_sync(FIL *file) { (void)file; ++syncs; return sync_error; }
FRESULT f_close(FIL *file) { (void)file; ++closes; return close_error; }

int main(void)
{
    static log_file_t log;
    log_record_t record = { .timestamp_ms = 0x12345678, .source = 0x1234,
        .length = 3, .payload = {1, 2, 3} };
    assert(log_file_open(NULL) == FR_INVALID_PARAMETER);
    collisions = 2;
    assert(log_file_open(&log) == FR_OK);
    assert(strcmp(opened_path, "0:/LOG00002.BIN") == 0);
    assert(output_size == 16 && memcmp(output, "STML\x01\x00\x10\x00", 8) == 0);
    assert(log_file_open(&log) == FR_INVALID_OBJECT);
    assert(log_file_append(&log, &record) == FR_OK);
    assert(output_size == 16);
    assert(log_file_sync(&log) == FR_OK && output_size == 35);
    const uint8_t prefix[] = {'R','E','C','1',0x78,0x56,0x34,0x12,0x34,0x12,3,0,1,2,3};
    assert(memcmp(output + 16, prefix, sizeof(prefix)) == 0);
    /* Independently computed CRC-32/ISO-HDLC (Python zlib reference). */
    const uint8_t checksum[] = {0xE9, 0xAB, 0xDA, 0xF9};
    assert(memcmp(output + 16 + sizeof(prefix), checksum, sizeof(checksum)) == 0);
    log.size = LOG_ROTATE_BYTES;
    assert(log_file_append(&log, &record) == FR_OK);
    assert(strcmp(log.path, "0:/LOG00003.BIN") == 0 && closes == 1);
    assert(log_file_close(&log) == FR_OK && output_size == 35);
    assert(log_file_append(&log, &record) == FR_INVALID_OBJECT);
    assert(log_file_close(&log) == FR_OK);
    assert(log_file_open(&log) == FR_OK);
    for (unsigned int i = 0; i < 300; ++i) assert(log_file_append(&log, &record) == FR_OK);
    assert(output_size > 16);
    assert(log_file_close(&log) == FR_OK && output_size == 16U + 300U * 19U);
    assert(log_file_open(&log) == FR_OK);
    assert(log_file_append(&log, &record) == FR_OK);
    short_write = true;
    assert(log_file_sync(&log) == FR_DENIED);
    unsigned int before = writes;
    assert(log_file_sync(&log) == FR_DENIED);
    assert(log_file_append(&log, &record) == FR_DENIED);
    assert(log_file_close(&log) == FR_DENIED && writes == before);
    short_write = false;
    assert(log_file_open(&log) == FR_OK);
    assert(log_file_append(&log, &record) == FR_OK);
    write_error = FR_DISK_ERR;
    assert(log_file_sync(&log) == FR_DISK_ERR);
    assert(log_file_close(&log) == FR_DISK_ERR);
    write_error = FR_OK;
    sync_error = FR_DISK_ERR;
    assert(log_file_open(&log) == FR_DISK_ERR && log.open);
    assert(log_file_close(&log) == FR_DISK_ERR);
    sync_error = FR_OK;
    assert(log_file_open(&log) == FR_OK);
    close_error = FR_DISK_ERR;
    assert(log_file_close(&log) == FR_DISK_ERR);
    puts("Log file tests passed: framing, buffering, rotation, disk full, write/sync/close errors.");
    return 0;
}
