# Shared sample clock

Every sensor uses this clock to timestamp its measurements. A sample is one
sensor measurement. The clock provides elapsed time for comparing samples and
UTC for placing them on a real-world timeline.

## Capture a timestamp

Call `sample_clock_now()` when the measurement becomes available:

```c
sample_timestamp_t timestamp = sample_clock_now();
```

Keep the result with the sample as it passes through the logger queue.

| Field | Meaning |
| --- | --- |
| `elapsed_ms` | 64-bit milliseconds since boot |
| `utc_ms` | Milliseconds since 1970-01-01 UTC, or zero when unavailable |
| `status` | Whether UTC is unsynchronized, GPS-based, or in holdover |

Use `status` to decide whether UTC is available. `SAMPLE_TIME_UNSYNCED` means there
is no valid UTC value. A CSV row should leave UTC empty in that case.

## How GPS sets UTC

The GPS parser calls `sample_clock_sync_utc()` with a pair of times: when the
message arrived on the elapsed clock, and the UTC reported by GPS. Other samples
use the time since that pair to estimate their UTC:

```text
sample UTC = GPS UTC + elapsed time since that GPS message
```

Updates with future reception times, nonpositive UTC, or repeated or older time
pairs are rejected. After 3 seconds without a new update, status changes from
`SAMPLE_TIME_GPS` to `SAMPLE_TIME_HOLDOVER`. UTC continues from the elapsed clock
until GPS returns.

GPS can correct UTC forward or backward. Elapsed time keeps increasing, and
previously captured timestamps stay unchanged. Use elapsed time to order samples
and measure intervals. See the [GNSS guide](../sensors/gnss_logging.md) for UART
accuracy limits.

## Timestamp buffered input

The UART driver saves a HAL tick with each byte. `sample_clock_from_tick()` uses
that saved tick to recover the acquisition timestamp after buffering or task
delays. The GNSS parser uses the first `$` byte's tick.

The saved tick must be less than one 32-bit wrap old, about 49.7 days. A reception
before the current GPS time pair is marked unsynchronized. An accepted RMC update
also sets that message's own UTC timestamp.

## How elapsed time works

The clock extends the existing 32-bit HAL tick to 64 bits. TIM1 calls
`sample_clock_tick()` after `HAL_IncTick()`, so wrap is handled even when sensors
are idle. No new task or hardware timer is needed.

Short interrupt masks protect shared values on the 32-bit MCU. The clock can be
read from tasks or interrupts, including before the scheduler starts.

Elapsed time follows the HAL tick. Suspending that tick, or masking interrupts
for too long, can stop it advancing. Keep the HAL tick running during recording.

The logger queue carries the full timestamp. The current binary writer saves
only its lower 32 elapsed-time bits; saving UTC to the card is part of the next
CSV stage.
