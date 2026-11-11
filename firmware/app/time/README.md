# Sample clock

The sample clock gives each measurement an elapsed time and, once GPS sets the
RTC, a UTC date and time. The RTC keeps running if GPS is disconnected.

## Capture a timestamp

Call `sample_clock_now()` when a measurement is ready:

```c
sample_timestamp_t timestamp = sample_clock_now();
```

| Field | Meaning |
| --- | --- |
| `elapsed_ms` | 64-bit milliseconds since boot |
| `utc` | RTC date and time, including milliseconds; use only when status is `SAMPLE_TIME_RTC` |
| `status` | `SAMPLE_TIME_UNSYNCED` or `SAMPLE_TIME_RTC` |

Keep the timestamp with the measurement through queueing and SD writes. Use
`elapsed_ms` for ordering and intervals. The GNSS task timestamps measurements
after decoding a complete RMC message.

## Initialize from GPS

The GNSS task calls `sample_clock_init_utc()` with a valid GPS calendar. Call it
only from that task, after `MX_RTC_Init()`, with interrupts enabled: HAL setup
can wait for hardware.

Timestamps stay `UNSYNCED` until setup succeeds. Failed attempts return false
and can be retried. After success, later GPS messages leave the RTC alone.
`sample_clock_utc_is_set()` reports whether setup succeeded during this boot.
Each reboot needs GPS again, even if the RTC retained its calendar.

The RTC supports 2000 to 2099 and handles date and leap-year rollover. Invalid dates
and times are rejected. See [GPS reception](../sensors/README.md#gps-reception)
for parsing rules.

## Timing details

The board's 32.768 kHz LSE crystal drives the RTC. Prescalers 31/1023 give a
1024 Hz subsecond counter. `HAL_RTCEx_SetSynchroShift()` sets the initial
fractional second, rounded to that resolution; `HAL_RTC_SetTime()` alone does not.

Reads briefly mask interrupts to capture elapsed time and the RTC together, then
restore the caller's interrupt mask. `HAL_RTC_GetTime()` is always followed by
`HAL_RTC_GetDate()` to release the calendar shadow registers, even after a read
error. These calls do not wait for hardware.

A failed or invalid RTC read makes that sample `UNSYNCED`. `RTC` means the clock
was read successfully; it does not indicate a current GPS fix. There is no GPS
timeout.

Initial time includes receiver, UART, and task delays. There is no PPS alignment.
Later drift depends on crystal tolerance and temperature, so measure it on the
board. UTC comes directly from the RTC, without Unix time conversion or software
interpolation.

## Elapsed time

TIM1 calls `sample_clock_tick()` after `HAL_IncTick()` to track 32-bit tick wraps.
No extra task is needed. Reads work from tasks or interrupts, including before
the scheduler starts. Keep the HAL tick running while recording; the RTC runs
independently.

See the [logging guide](../logging/README.md#gps-csv-format) for recorded fields.
