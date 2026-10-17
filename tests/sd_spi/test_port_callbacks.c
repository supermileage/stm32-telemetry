/* This target intentionally has no STM32 headers or BSP sources. */
#include "sd_spi.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static uint32_t tick, transfers;
static bool selected, fail_exchange, fail_speed;

static bool exchange(uint8_t tx, uint8_t *rx)
{
    (void)tx;
    ++transfers;
    ++tick;
    *rx = 0xFF; /* An absent card never answers CMD0. */
    return !fail_exchange;
}

static bool set_speed(uint32_t maximum_hz)
{
    assert(maximum_hz == 400000U);
    return !fail_speed;
}

static void chip_select(bool active)
{
    selected = active;
}

static uint32_t get_tick_ms(void)
{
    return tick;
}

static void delay_ms(uint32_t delay)
{
    tick += delay;
}

int main(void)
{
    const sd_spi_port_t callbacks = {
        .exchange = exchange, .set_speed = set_speed, .chip_select = chip_select,
        .get_tick_ms = get_tick_ms, .delay_ms = delay_ms
    };
    assert(sd_spi_initialize() == SD_NOT_READY && transfers == 0U);
    assert(!sd_spi_bind(NULL));
    for (unsigned int missing = 0U; missing < 5U; ++missing) {
        sd_spi_port_t incomplete = callbacks;
        switch (missing) {
        case 0U:
            incomplete.exchange = NULL;
            break;
        case 1U:
            incomplete.set_speed = NULL;
            break;
        case 2U:
            incomplete.chip_select = NULL;
            break;
        case 3U:
            incomplete.get_tick_ms = NULL;
            break;
        case 4U:
            incomplete.delay_ms = NULL;
            break;
        }
        assert(!sd_spi_bind(&incomplete));
        assert(sd_spi_initialize() == SD_NOT_READY && transfers == 0U);
    }
    sd_spi_port_t temporary = callbacks;
    assert(sd_spi_bind(&temporary));
    temporary = (sd_spi_port_t){0}; /* Binding must copy the callback table. */
    assert(sd_spi_initialize() == SD_NOT_READY && transfers > 0U && !selected);

    fail_exchange = true;
    sd_spi_diagnostics_t before, after;
    sd_spi_get_diagnostics(&before);
    uint32_t initial_transfers = transfers;
    assert(sd_spi_initialize() == SD_ERROR && !selected);
    sd_spi_get_diagnostics(&after);
    assert(transfers == initial_transfers + 1U);
    assert(after.transfer_failures == before.transfer_failures + 1U);
    assert(after.last_result == SD_ERROR);

    fail_exchange = false;
    fail_speed = true;
    initial_transfers = transfers;
    sd_spi_get_diagnostics(&before);
    assert(sd_spi_initialize() == SD_NOT_READY && !selected);
    sd_spi_get_diagnostics(&after);
    assert(transfers == initial_transfers);
    assert(after.speed_failures == before.speed_failures + 1U);
    assert(!sd_spi_bind(NULL));
    assert(sd_spi_sync() == SD_NOT_READY);
    assert(sd_spi_initialize() == SD_NOT_READY && transfers == initial_transfers);
    puts("Portable SD callbacks passed: validation, copied lifetime, failure propagation, and unbind.");
    return 0;
}
