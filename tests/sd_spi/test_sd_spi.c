#include "sd_spi.h"
#include "sd_spi_port.h"
#include "storage.h"
#include "ff_gen_drv.h"
#include "log_file.h"
#include "sd_diskio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

SPI_TypeDef mock_spi1, mock_spi2, mock_spi3;
GPIO_TypeDef gpio;
SPI_HandleTypeDef hspi1;
extern Disk_drvTypeDef disk;
#define spi hspi1


/* A byte-level SD model: commands, streamed sectors, write tokens and busy time. */
static struct {
    uint32_t tick, clock, transfers, high_clocks, command_count[64], arguments[64];
    uint32_t init_calls, fail_init_call, acmd_calls, busy_remaining;
    bool selected, idle, legacy, block_addressed, absent;
    bool wrong_echo, wrong_cmd8_response, ocr_not_ready;
    bool init_never_ready, missing_token, read_error;
    bool reject_write, status_error, force_busy, read_stream, write_stream;
    bool bad_crc, bad_dummy, stop_seen;
    int fail_transfer_after;
    uint8_t packet[6], response[1024], memory[8192][512];
    size_t packet_size, response_head, response_tail;
    uint32_t read_sector, write_sector, write_remaining;
    uint32_t stop_commands, multi_stops;
} model;

static void enqueue(uint8_t byte)
{
    assert(model.response_tail < sizeof(model.response));
    model.response[model.response_tail++] = byte;
}

static uint8_t dequeue(void)
{
    if (model.response_head == model.response_tail) {
        return 0xFF;
    }
    uint8_t result = model.response[model.response_head++];
    if (model.response_head == model.response_tail) {
        model.response_head = model.response_tail = 0;
    }
    return result;
}

static void enqueue_sector(void)
{
    if (model.missing_token) {
        return;
    }
    if (model.read_error) {
        enqueue(0x0B);
        model.read_stream = false;
        return;
    }
    assert(model.read_sector < 8192);
    enqueue(0xFF); /* Nac delay before the data token. */
    enqueue(0xFE);
    for (size_t i = 0; i < 512; ++i) {
        enqueue(model.memory[model.read_sector][i]);
    }
    enqueue(0x12);
    enqueue(0x34); /* Both CRC bytes must be consumed. */
    ++model.read_sector;
}

static void handle_command(void)
{
    uint8_t cmd = model.packet[0] & 0x3F;
    uint32_t argument = ((uint32_t)model.packet[1] << 24) |
        ((uint32_t)model.packet[2] << 16) | ((uint32_t)model.packet[3] << 8) |
        model.packet[4];
    ++model.command_count[cmd];
    model.arguments[cmd] = argument;
    if ((cmd == 0 && model.packet[5] != 0x95) ||
        (cmd == 8 && model.packet[5] != 0x87)) {
        model.bad_crc = true;
    }
    model.packet_size = 0;
    switch (cmd) {
    case 0:
        assert(model.high_clocks >= 80);
        assert(spi.Init.BaudRatePrescaler == 64);
        model.idle = true;
        enqueue(1);
        break;
    case 8:
        assert(argument == 0x1AA);
        enqueue(model.wrong_cmd8_response ? 2 : model.legacy ? 5 : 1);
        if (!model.legacy && !model.wrong_cmd8_response) {
            enqueue(0); enqueue(0); enqueue(1);
            enqueue(model.wrong_echo ? 0xAB : 0xAA);
        }
        break;
    case 55:
        enqueue(model.idle ? 1 : 0);
        break;
    case 41:
        assert(argument == (model.legacy ? 0 : 0x40000000U));
        if (!model.init_never_ready && ++model.acmd_calls >= 2) {
            model.idle = false;
        }
        enqueue(model.idle ? 1 : 0);
        break;
    case 58:
        enqueue(0);
        enqueue((model.ocr_not_ready ? 0 : 0x80) | (model.block_addressed ? 0x40 : 0));
        enqueue(0xFF); enqueue(0x80); enqueue(0);
        break;
    case 16:
        assert(argument == 512 && !model.block_addressed);
        enqueue(0);
        break;
    case 9: {
        uint8_t csd[16] = {0};
        if (model.block_addressed) {
            csd[0] = 0x40;
            csd[9] = 7; /* (C_SIZE + 1) * 1024 = 8192 sectors. */
        } else {
            csd[5] = 9; csd[6] = 1; csd[7] = 0xFF; csd[8] = 0xC0;
        }
        enqueue(0); enqueue(0xFE);
        for (size_t i = 0; i < sizeof(csd); ++i) enqueue(csd[i]);
        enqueue(0x12); enqueue(0x34);
        break;
    }
    case 23:
        assert(argument > 0);
        enqueue(0);
        break;
    case 17:
    case 18:
        model.read_sector = model.block_addressed ? argument : argument / 512;
        enqueue(0);
        model.read_stream = cmd == 18;
        if (cmd == 17) {
            enqueue_sector();
        }
        break;
    case 12:
        model.read_stream = false;
        model.response_head = model.response_tail = 0;
        ++model.stop_commands;
        enqueue(0x7F); /* Stuff byte is not an R1 response. */
        enqueue(0);
        model.busy_remaining = 2;
        break;
    case 24:
    case 25:
        model.write_sector = model.block_addressed ? argument : argument / 512;
        model.write_stream = cmd == 25;
        enqueue(0);
        break;
    case 13:
        enqueue(0);
        enqueue(model.status_error ? 4 : 0);
        break;
    default:
        assert(!"Unexpected SD command");
    }
}

static uint8_t clock_byte(uint8_t tx)
{
    ++model.tick;
    ++model.transfers;
    if (!model.selected) {
        assert(tx == 0xFF);
        model.high_clocks += 8;
        return 0xFF;
    }
    if (model.absent) {
        return 0xFF;
    }
    if (model.write_remaining != 0) {
        uint32_t offset = 514 - model.write_remaining;
        assert(model.write_sector < 8192);
        if (offset < 512) {
            model.memory[model.write_sector][offset] = tx;
        }
        if (--model.write_remaining == 0) {
            ++model.write_sector;
            enqueue(model.reject_write ? 0x0D : 0x05);
            model.busy_remaining = 3;
        }
        return 0xFF;
    }
    if (model.packet_size != 0) {
        model.packet[model.packet_size++] = tx;
        uint8_t rx = dequeue();
        if (model.packet_size == 6) {
            handle_command();
        }
        return rx;
    }
    if ((tx & 0xC0) == 0x40) {
        model.packet[0] = tx;
        model.packet_size = 1;
        return dequeue();
    }
    if (tx == 0xFE || tx == 0xFC) {
        assert(tx == (model.write_stream ? 0xFC : 0xFE));
        assert(model.response_head == model.response_tail && model.busy_remaining == 0);
        model.write_remaining = 514;
        return 0xFF;
    }
    if (tx == 0xFD) {
        assert(model.write_stream);
        model.write_stream = false;
        ++model.multi_stops;
        model.busy_remaining = 2;
        return 0xFF;
    }
    if (tx != 0xFF) {
        model.bad_dummy = true;
    }
    if (model.response_head != model.response_tail) {
        return dequeue();
    }
    if (model.force_busy || model.busy_remaining != 0) {
        if (model.busy_remaining != 0) {
            --model.busy_remaining;
        }
        return 0;
    }
    if (model.read_stream) {
        enqueue_sector();
        return dequeue();
    }
    return 0xFF;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *handle,
    const uint8_t *tx, uint8_t *rx, uint16_t count, uint32_t timeout)
{
    assert(handle == &spi && tx != NULL && rx != NULL && count != 0);
    assert(timeout != 0 && timeout != UINT32_MAX);
    for (uint16_t i = 0; i < count; ++i) {
        if (model.fail_transfer_after == 0) {
            return HAL_TIMEOUT;
        }
        if (model.fail_transfer_after > 0) {
            --model.fail_transfer_after;
        }
        rx[i] = clock_byte(tx[i]);
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *handle)
{
    assert(handle == &spi && !model.selected);
    assert(handle->Init.NSSPMode == SPI_NSS_PULSE_DISABLE);
    assert(handle->Init.MasterKeepIOState == SPI_MASTER_KEEP_IO_STATE_ENABLE);
    return ++model.init_calls == model.fail_init_call ? HAL_ERROR : HAL_OK;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    assert(port == &gpio && pin == (1U << 12));
    model.selected = state == GPIO_PIN_RESET;
    if (!model.selected) {
        model.packet_size = 0;
        model.response_head = model.response_tail = 0;
    }
}

uint32_t HAL_GetTick(void) { return model.tick; }
void HAL_Delay(uint32_t delay) { model.tick += delay; }
uint32_t HAL_RCCEx_GetPeriphCLKFreq(uint64_t clock)
{
    assert(clock == RCC_PERIPHCLK_SPI1);
    return model.clock;
}

static void reset_model(bool legacy, bool block_addressed)
{
    memset(&model, 0, sizeof(model));
    spi = (SPI_HandleTypeDef){ .Instance = SPI1, .Init = {
        .Mode = SPI_MODE_MASTER, .Direction = SPI_DIRECTION_2LINES,
        .DataSize = SPI_DATASIZE_8BIT, .FirstBit = SPI_FIRSTBIT_MSB,
        .CLKPolarity = SPI_POLARITY_LOW, .CLKPhase = SPI_PHASE_1EDGE,
        .NSS = SPI_NSS_SOFT, .TIMode = SPI_TIMODE_DISABLE,
        .CRCCalculation = SPI_CRCCALCULATION_DISABLE, .NSSPMode = 1
    }};
    model.clock = 16000000;
    model.legacy = legacy;
    model.block_addressed = block_addressed;
    model.fail_transfer_after = -1;
    for (uint32_t sector = 0; sector < 8; ++sector) {
        for (uint32_t i = 0; i < 512; ++i) {
            model.memory[sector][i] = (uint8_t)(sector * 17 + i);
        }
    }
}

static bool initialize(void)
{
    return sd_spi_port_bind(&spi, &gpio, 1U << 12) && sd_spi_initialize() == SD_OK;
}

static bool ready(void) { return sd_spi_is_ready(); }
static bool read_sectors(uint32_t sector, uint8_t *buffer, uint32_t count)
{
    return sd_spi_read(sector, buffer, count) == SD_OK;
}
static bool write_sectors(uint32_t sector, const uint8_t *buffer, uint32_t count)
{
    return sd_spi_write(sector, buffer, count) == SD_OK;
}
static bool sync_card(void) { return sd_spi_sync() == SD_OK; }

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
}

static void format_mock_fat16(void)
{
    memset(model.memory, 0, sizeof(model.memory));
    uint8_t *boot = model.memory[0];
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;
    memcpy(boot + 3, "SDMOCK  ", 8);
    put16(boot + 11, 512); boot[13] = 1;
    put16(boot + 14, 1); boot[16] = 2;
    put16(boot + 17, 512); put16(boot + 19, 8192);
    boot[21] = 0xF8; put16(boot + 22, 32);
    put16(boot + 24, 32); put16(boot + 26, 64);
    boot[36] = 0x80; boot[38] = 0x29;
    memcpy(boot + 43, "SD TEST    ", 11);
    memcpy(boot + 54, "FAT16   ", 8);
    boot[510] = 0x55; boot[511] = 0xAA;
    put16(model.memory[1], 0xFFF8); put16(model.memory[1] + 2, 0xFFFF);
    put16(model.memory[33], 0xFFF8); put16(model.memory[33] + 2, 0xFFFF);
}

static void assert_released(void)
{
    assert(!model.selected && !model.bad_crc && !model.bad_dummy);
}

int main(void)
{
    uint8_t buffer[1024], payload[1024];
    memset(payload, 0xA5, sizeof(payload));

    /* SDHC, SD v2 SDSC, and legacy SDSC follow their distinct init/addressing paths. */
    for (uint32_t type = 0; type < 3; ++type) {
        reset_model(type == 2, type == 0);
        assert(initialize() && ready());
        assert(spi.Init.BaudRatePrescaler == 4);
        assert(model.command_count[16] == (type == 0 ? 0U : 1U));
        assert(read_sectors(2, buffer, 1));
        assert(memcmp(buffer, model.memory[2], 512) == 0);
        assert(model.arguments[17] == (type == 0 ? 2U : 1024U));
        assert(read_sectors(3, buffer, 2));
        assert(memcmp(buffer, model.memory[3], 512) == 0);
        assert(memcmp(buffer + 512, model.memory[4], 512) == 0);
        assert(model.stop_commands == 1);
        assert(write_sectors(1, payload, 1));
        assert(memcmp(model.memory[1], payload, 512) == 0);
        assert(model.arguments[24] == (type == 0 ? 1U : 512U));
        assert(write_sectors(5, payload, 2));
        assert(memcmp(model.memory[5], payload, 512) == 0);
        assert(memcmp(model.memory[6], payload + 512, 512) == 0);
        assert(model.multi_stops == 1 && model.command_count[13] == 2);
        assert(model.busy_remaining == 0 && sync_card());
        uint32_t sectors = 0;
        assert(sd_spi_get_sector_count(&sectors) == SD_OK && sectors == 8192);
        assert(SD_SPI_SECTOR_SIZE == 512);
        assert(sd_spi_get_sector_count(NULL) == SD_INVALID_ARGUMENT);
        assert_released();
    }

    reset_model(false, true);
    model.clock = 160000000; /* /256 still exceeds the initialization limit. */
    assert(!initialize() && !ready() && model.transfers == 0);
    assert_released();

    reset_model(false, true);
    spi.Init.DataSize = 4;
    assert(!initialize() && !ready() && model.transfers == 0);

    reset_model(false, true);
    spi.Instance = NULL;
    assert(!initialize() && !ready() && model.transfers == 0);

    reset_model(false, true);
    model.absent = true;
    assert(!initialize() && !ready() && model.tick < 1000);
    assert_released();

    reset_model(false, true);
    model.wrong_echo = true;
    assert(!initialize() && !ready());
    assert_released();

    reset_model(false, true);
    model.wrong_cmd8_response = true;
    assert(!initialize() && !ready());
    assert(model.command_count[41] == 0U && model.command_count[1] == 0U);
    assert_released();

    reset_model(false, true);
    model.ocr_not_ready = true;
    assert(!initialize() && !ready() && model.init_calls == 1U);
    assert_released();

    reset_model(false, true);
    model.init_never_ready = true;
    assert(!initialize() && !ready() && model.tick >= 1000);
    assert_released();

    reset_model(false, true);
    model.fail_init_call = 2; /* Failure when switching to transfer speed. */
    assert(!initialize() && !ready());
    assert_released();

    reset_model(false, true);
    assert(initialize());
    uint32_t transfers = model.transfers;
    assert(!read_sectors(0, NULL, 1));
    assert(!write_sectors(0, payload, 0));
    assert(!read_sectors(UINT32_MAX, buffer, 2));
    assert(sd_spi_read(0, buffer, 0) == SD_INVALID_ARGUMENT);
    assert(sd_spi_get_sector_count(NULL) == SD_INVALID_ARGUMENT);
    assert(ready() && model.transfers == transfers);
    reset_model(false, false);
    assert(initialize());
    transfers = model.transfers;
    assert(!read_sectors(UINT32_MAX / 512, buffer, 2));
    assert(ready() && model.transfers == transfers);

    reset_model(false, true);
    assert(initialize());
    model.tick = UINT32_MAX - 100; /* Timeout arithmetic must survive wraparound. */
    model.missing_token = true;
    assert(!read_sectors(0, buffer, 1) && !ready());
    assert_released();

    reset_model(false, true);
    assert(initialize());
    model.read_error = true;
    assert(!read_sectors(0, buffer, 2) && !ready());
    assert(model.stop_commands == 1);
    assert_released();

    reset_model(false, true);
    assert(initialize());
    model.reject_write = true;
    assert(!write_sectors(0, payload, 2) && !ready());
    assert(model.multi_stops == 1);
    assert_released();

    reset_model(false, true);
    assert(initialize());
    model.status_error = true;
    assert(!write_sectors(0, payload, 1) && !ready());
    assert_released();

    reset_model(false, true);
    assert(initialize());
    model.force_busy = true;
    transfers = model.transfers;
    assert(!sync_card() && !ready());
    assert(model.transfers - transfers <= 1002);
    assert_released();

    reset_model(false, true);
    assert(initialize());
    model.fail_transfer_after = 30;
    sd_spi_diagnostics_t before_failure, after_failure;
    sd_spi_get_diagnostics(&before_failure);
    assert(!read_sectors(0, buffer, 1) && !ready());
    sd_spi_get_diagnostics(&after_failure);
    assert(after_failure.read_failures == before_failure.read_failures + 1U);
    assert(after_failure.transfer_failures == before_failure.transfer_failures + 1U);
    assert(after_failure.last_result == SD_ERROR);
    transfers = model.transfers;
    assert(sd_spi_read(0, buffer, 1) == SD_NOT_READY && model.transfers == transfers);
    sd_spi_get_diagnostics(&after_failure);
    assert(after_failure.not_ready_requests == before_failure.not_ready_requests + 1U);
    sd_spi_get_diagnostics(NULL);
    assert_released();

    /* Real FatFs calls traverse the adapter, port and byte-level SPI model. */
    reset_model(false, true);
    format_mock_fat16();
    assert(storage_mount() == FR_OK);
    assert(storage_mount() == FR_OK); /* No duplicate driver registration. */
    /* Drive validation and type conversion belong to the FatFs adapter. */
    assert(disk.drv[0]->disk_initialize(1) == STA_NOINIT);
    assert(disk.drv[0]->disk_status(1) == STA_NOINIT);
    assert(disk.drv[0]->disk_read(1, buffer, 0, 1) == RES_PARERR);
    assert(disk.drv[0]->disk_write(1, buffer, 0, 1) == RES_PARERR);
    assert(disk.drv[0]->disk_read(0, NULL, 0, 1) == RES_PARERR);
    assert(disk.drv[0]->disk_ioctl(1, CTRL_SYNC, NULL) == RES_PARERR);
    assert(disk.drv[0]->disk_ioctl(0, GET_SECTOR_COUNT, NULL) == RES_PARERR);
    assert(disk.drv[0]->disk_ioctl(0, CTRL_TRIM, buffer) == RES_PARERR);
    WORD sector_size = 0;
    DWORD sectors = 0, block_size = 0;
    assert(disk_ioctl(0, GET_SECTOR_SIZE, &sector_size) == RES_OK && sector_size == 512);
    assert(disk_ioctl(0, GET_BLOCK_SIZE, &block_size) == RES_OK && block_size == 1);
    assert(disk_ioctl(0, GET_SECTOR_COUNT, &sectors) == RES_OK && sectors == 8192);
    char first_path[17], second_path[17];
    assert(storage_self_test(first_path) == FR_OK);
    assert(strcmp(first_path, "0:/HELLO000.TXT") == 0);
    assert(storage_self_test(second_path) == FR_OK);
    assert(strcmp(second_path, "0:/HELLO001.TXT") == 0);
    FILINFO info;
    assert(f_stat(first_path, &info) == FR_OK);
    assert(info.fsize == strlen("Hello World\r\n"));
    FIL hello_reader;
    UINT hello_bytes;
    char hello_contents[32] = {0};
    assert(f_open(&hello_reader, first_path, FA_READ) == FR_OK);
    assert(f_read(&hello_reader, hello_contents, sizeof(hello_contents), &hello_bytes) == FR_OK);
    assert(hello_bytes == 13U && memcmp(hello_contents, "Hello World\r\n", 13U) == 0);
    assert(f_close(&hello_reader) == FR_OK);
    assert(FATFS_GetAttachedDriversNbr() == 1);
    static log_file_t log;
    log_record_t record = { .timestamp_ms = 12345, .source = 1,
        .length = 4, .payload = {0x01, 0x02, 0x03, 0x04} };
    assert(log_file_open(&log) == FR_OK);
    assert(strcmp(log.path, "0:/LOG00000.BIN") == 0);
    for (unsigned int i = 0; i < 300; ++i) assert(log_file_append(&log, &record) == FR_OK);
    assert(log_file_sync(&log) == FR_OK);
    assert(log_file_close(&log) == FR_OK);
    assert(f_stat(log.path, &info) == FR_OK && info.fsize == 16U + 300U * 20U);
    FIL log_reader;
    UINT bytes;
    uint8_t header_and_frame[36];
    assert(f_open(&log_reader, log.path, FA_READ) == FR_OK);
    assert(f_read(&log_reader, header_and_frame, sizeof(header_and_frame), &bytes) == FR_OK);
    assert(bytes == sizeof(header_and_frame));
    assert(memcmp(header_and_frame, "STML", 4) == 0);
    assert(memcmp(header_and_frame + 16, "REC1", 4) == 0);
    assert(header_and_frame[20] == 0x39 && header_and_frame[21] == 0x30);
    assert(memcmp(header_and_frame + 28, record.payload, 4) == 0);
    assert(f_close(&log_reader) == FR_OK);
    assert(log_file_open(&log) == FR_OK && strcmp(log.path, "0:/LOG00001.BIN") == 0);
    assert(log_file_close(&log) == FR_OK);
    assert(storage_unmount() == FR_OK && FATFS_GetAttachedDriversNbr() == 0);

    reset_model(false, true);
    model.absent = true;
    assert(storage_mount() == FR_NOT_READY && FATFS_GetAttachedDriversNbr() == 0);
    model.absent = false;
    format_mock_fat16();
    assert(storage_mount() == FR_OK); /* Retry resets ST's cached init flag. */
    assert(storage_unmount() == FR_OK);

    reset_model(false, true);
    memset(model.memory, 0, sizeof(model.memory));
    assert(storage_mount() == FR_NO_FILESYSTEM && FATFS_GetAttachedDriversNbr() == 0);
    assert(storage_self_test(first_path) == FR_NOT_READY);
    assert(storage_unmount() == FR_OK);

    puts("SD SPI tests passed: upstream port, failures, capacity, and FatFs file round trips.");
    return 0;
}
