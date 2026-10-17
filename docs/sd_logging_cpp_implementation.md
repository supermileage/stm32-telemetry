# Implement SD telemetry logging in C++

This plan describes future C++ SD/logging work. Startup and the SPI BSP
(milestones 1 and 2) are implemented; the later C++ modules and decoder are
not. The existing C storage/logger is still covered by host tests, but is not
started by the application or included in the CMake firmware target.

Build and verify each milestone before adding the next layer. New application
code uses C++17; HAL, FreeRTOS, FatFs, and the existing SD protocol driver stay C.

## Architecture and boundaries

```text
sensor task -> logging::submit(record) -> static command queue
  -> logger task -> LogWriter -> FatFs Volume/disk adapter
  -> sd_spi.c -> sd_spi_port_t callbacks -> bsp::SdSpiPort -> STM32 HAL
```

One logger task owns all filesystem and SD operations. Producers submit values,
never file handles or pointers to temporary payloads. Use namespaces for service
functions and classes for objects that own state. Avoid a class for every helper.

First release: one dedicated SPI bus, one mounted volume, no hot-plug/restart,
fixed-size records, static allocation, explicit shutdown. Use return values for
errors. Constructors initialize fields only; hardware access belongs in explicit
initialization methods. Disable exceptions and RTTI for application targets.
Use `std::array` and plain structs; do not require `new`, `std::string`, containers
that allocate, or `std::function`. Port, volume, and writer objects are noncopyable
and nonmovable. Their long-lived storage outlasts tasks and adapter callbacks.

Create these files progressively, not all at once:

```text
firmware/
  app/
    app.h                          C-compatible startup declaration
    app.cpp                        C++ service startup
    storage/
      volume.hpp / .cpp             mount/unmount ownership
      sd_card_test.cpp              card bring-up diagnostic
      sd_smoke_test.cpp             filesystem diagnostic
    logging/
      config.hpp                   buffer/task/queue policy
      record.hpp                   producer value type
      log_format.hpp / .cpp         bytes, little endian, CRC
      log_writer.hpp / .cpp         file buffering and explicit close
      logger.hpp / .cpp             public service and private task/queue
  BSP/
    sd_spi_port.h / .cpp            C binding and C++ STM32 SPI bus
    b_u585i_iot02a/
      board_sd.h / .cpp             SPI handle, CS pin, callback binding
lib/drivers/sd_spi/
  sd_spi.c / sd_spi.h               existing protocol and callback API; reuse
  fatfs/sd_diskio.c / .h            existing reusable FatFs adapter; reuse
tools/
  log_decoder.py                   independent reader
tests/
  sd_spi/test_sd_spi_port.cpp
  logging/test_log_format.cpp
  logging/test_log_writer.cpp
  logging/test_logger.cpp
```

## Milestone 1: establish C++ startup and build support

For exact CubeIDE clicks, startup code, and CMake edits, see
[the milestone 1 walkthrough](cpp_milestone_1.md).

Change CMake's project languages to `C CXX`; set the new targets to C++17, with
C++ extensions disabled. Add `arm-none-eabi-g++` to the Arm toolchain file and
`g++` to the native host-test configuration. Compile new `.cpp` sources with the
C++ compiler and ensure the final firmware link uses the C++ driver. Apply
`-fno-exceptions -fno-rtti` only to C++ sources, not to HAL/FreeRTOS/FatFs C files.
Keep the current CPU, floating-point, linker-script, and runtime settings.

In CubeIDE, set C++17, matching include paths/defines, and the C++ linker in both
Debug and Release configurations. Add new sources to the explicit CMake list.
When replacing old files, remove their old implementations from the build.

Create `app/app.h`:

```cpp
#ifndef TELEMETRY_APP_H
#define TELEMETRY_APP_H

#ifdef __cplusplus
extern "C" {
#endif
void app_init(void);
#ifdef __cplusplus
}
#endif

#endif
```

The original milestone used an empty `app_init()` in `app/app.cpp`.
Keep the current SPI binding when extending that function.
Call it from the generated RTOS initialization USER CODE block before scheduler
start. FatFs and FreeRTOS headers in this checkout already have C++ linkage
guards; use a local compatibility wrapper if another C library lacks them.

**Pass:** firmware links, reaches `app_init()`, and runs the existing scheduler.
No SD operations yet.

## Milestone 2: connect the STM32 SPI port to the existing driver

This stage is implemented. See [the BSP guide](../firmware/BSP/README.md).
Keep the existing hardware boundary: `sd_spi_port_t` in `sd_spi.h`. The C SD
driver already handles card commands, addressing, sector I/O, and readiness.

Use `BSP/sd_spi_port.h/.cpp`, a concrete `bsp::SdSpiPort` with:

```cpp
bool configure(SPI_HandleTypeDef& spi, GPIO_TypeDef& csPort,
               std::uint16_t csPin) noexcept;
bool exchange(std::uint8_t tx, std::uint8_t* rx) noexcept;
void select(bool active) noexcept;
bool setMaxFrequency(std::uint32_t hz) noexcept;
```

The single header declares `sd_spi_port_bind()` with C linkage and guards the
C++ class with `#ifdef __cplusplus`, so both C and C++ callers can include it.

Configure validates initialized mode-0, 8-bit SPI, sets CS high, and chooses a
startup clock at or below 400 kHz. Exchange performs one blocking byte transfer
with a finite timeout. Speed selection uses the actual kernel clock and refuses
changes while CS is active. Keep the STM32U5 IO-retention settings.

`board_sd.h/.cpp` supplies `bsp::configureSd()` and names the SPI1 handle and
CS pin. Its header also exposes `board_sd_configure()` to C callers and guards
the C++ declaration with `#ifdef __cplusplus`. `sd_spi_port.cpp` owns the static
port and the forwarding callbacks for
exchange, speed, and CS. The time callbacks use `HAL_GetTick` and `HAL_Delay`
directly. `sd_spi_port_bind()` configures the port and calls `sd_spi_bind()`;
configuration failure clears the previous binding.

The C driver copies the callback table. The port must outlive every driver call.
Call configuration from `app_init()` after generated GPIO/SPI setup. Include
`sd_spi.h` directly from C++; its linkage guards preserve the C ABI.

**Pass:** configuration succeeds without accessing the card; host tests verify
the real C driver uses the C++ callbacks and handles absent cards and HAL errors.
Verify board CS, clocks, and signals with a debugger/logic analyzer.

## Milestone 3: bring up the existing SD block driver

Create a diagnostic-only `app/storage/sd_card_test.cpp` task. Use the existing
driver directly from C++:

```cpp
#include "sd_spi.h"
#include <cstdint>

sd_result_t result = sd_spi_initialize();
bool ready = sd_spi_is_ready();
std::uint32_t sectors{};
result = sd_spi_get_sector_count(&sectors);
std::uint8_t sector[SD_SPI_SECTOR_SIZE]{};
result = sd_spi_read(0, sector, 1);
```

Check each result before continuing to the next operation. Keep the sector
buffer in static storage if task-stack space is limited. First confirm card
initialization, capacity, and a read of sector 0. Use `sd_spi_write()` and
`sd_spi_sync()` later through the filesystem; raw write experiments require a
disposable card or explicitly reserved region.

The board binding is already established by `app_init()`; do not duplicate it
in every card operation. One task owns all driver calls. An I/O failure clears
readiness. Close files and unmount before reinitializing or rebinding.

Reuse the existing SD protocol tests and diagnostics. Measure throughput before
changing the byte callback interface. Keep protocol work in the maintained C
driver if a demonstrated bug needs fixing.

**Pass:** a real card initializes and reports sensible capacity; sector 0 reads
successfully. Missing-card and I/O failures are reported without hanging.

## Milestone 4: connect FatFs and run a file smoke test

Reuse the C-compatible adapter in
[`lib/drivers/sd_spi/fatfs`](../lib/drivers/sd_spi/fatfs/README.md):

```c
#include "sd_diskio.h"
bool sd_diskio_register(char path[4]);
bool sd_diskio_unregister(char path[4]);
```

It already provides private initialize/status/read/write/ioctl callbacks,
result translation, and `CTRL_SYNC`, sector-count, sector-size, and block-size
queries through ST's dispatcher. Compile its C source separately from the
protocol-only `sd_spi` target and add its directory to the include path. Include
the header directly from C++; its linkage guards preserve the C ABI. Keep this
reusable adapter in `lib` and avoid a second disk implementation in `app`.

Create `volume.hpp/.cpp`, class `storage::Volume`, owning `FATFS`, the drive path,
and registration/mount flags:

```cpp
FRESULT mount() noexcept;
FRESULT unmount() noexcept;
bool mounted() const noexcept;
const char* drivePath() const noexcept;
```

Mount calls `sd_diskio_register()` and `f_mount()`. Failure unwinds registration.
Unmount requires all files closed, unmounts FatFs, then calls
`sd_diskio_unregister()`. Update flags
only when the corresponding operation succeeds. Never automatically format a
card. Use FAT32 for the initial filesystem milestone; an SDXC card may require
preparation because the selected FatFs configuration does not enable exFAT.

Create a diagnostic-only `sd_smoke_test.cpp`: `runSdSmokeTest()` uses the startup
board binding, mounts, creates a new test file, writes a small message, checks the byte
count, syncs, closes, reopens, reads/compares, closes, and unmounts. Return the
first error. Run this in its own diagnostic task/build, never concurrently with
the eventual logger. Normal logging must not create a diagnostic file each boot.

**Pass:** the card contains the expected message and can be read on a computer.
Test missing card and unsupported filesystem without formatting or hanging.

## Milestone 5: define records and an independently readable format

Create `logging/record.hpp`:

```cpp
namespace logging {
inline constexpr std::size_t MaxPayload = 64;
struct Record {
    std::uint32_t timestampMs{};
    std::uint16_t source{};
    std::uint16_t length{};
    std::array<std::uint8_t, MaxPayload> payload{};
};
}
```

Add `static_assert(std::is_trivially_copyable_v<Record>)`. Define source IDs and
payload field encodings explicitly. Timestamps are milliseconds since boot;
document wraparound. Never write the memory representation of `Record` to disk.

Create `log_format.hpp/.cpp` with these pure functions:

```cpp
std::size_t encodeFileHeader(std::uint8_t* out, std::size_t capacity) noexcept;
std::size_t encodeRecord(const Record& record, std::uint8_t* out,
                         std::size_t capacity) noexcept;
std::uint32_t crc32(const std::uint8_t* bytes, std::size_t count) noexcept;
```

Return bytes produced, or zero for invalid length/pointer or insufficient room.
Document a versioned header and framed record containing timestamp, source,
length, payload, and CRC. A practical initial format is the 16-byte STML header
and REC1 frames described in `docs/logging_format.md`; adopt that specification
deliberately without copying C struct layout. Add named sizes rather than magic
offsets scattered through the writer.

Create `tools/log_decoder.py` now. Use Python `struct` and `zlib.crc32` to check
the format independently. Stop at the first damaged/truncated frame and retain
earlier valid records. Add `test_log_format.cpp` and decoder tests.

**Pass:** independently checked bytes round-trip, and empty/max payloads,
invalid lengths, CRC corruption, and truncated frames behave as documented.

## Milestone 6: implement a synchronous file writer

Create `log_writer.hpp/.cpp`, class `logging::LogWriter`, with a `FIL` and private
open/error/buffer state. Implement:

```cpp
FRESULT open(const char* drivePath) noexcept;
FRESULT append(const Record& record) noexcept;
FRESULT sync() noexcept;
FRESULT close() noexcept;
const char* path() const noexcept;
```

Implement first with direct frame writes so that format and file I/O are easy to
debug. Then add a fixed 4 KiB buffer and private `flushBuffer()`. Validate that a
maximum frame fits the buffer. Append must flush when the next complete frame
will not fit. Sync must flush the application buffer before calling `f_sync()`.
Always check both `FRESULT` and the number of bytes written. A short write is an
error even when FatFs returns success. Latch the first writer error; do not retry
partially written frames blindly.

Use create-new filenames, e.g. `LOG00000.BIN`, without overwriting existing logs.
Detect exhausted names. Add private `openNextFile()` and rotation at a named
limit, e.g. 64 MiB, after basic append/sync/close works. Rotate between records,
close/sync the previous file, and write a header to every new file.

Close flushes and calls `f_close()`, returning the first error. A failed close is
not successful resource release. Require volume teardown/reboot on failure in
the first version. Destructors must not perform filesystem I/O: explicit close
is necessary because shutdown errors must be observable and global destruction
is not the embedded shutdown mechanism.

**Pass:** host tests cover short writes, buffering boundaries, disk full,
open/write/sync/close failures, naming, and rotation. A synchronous board test
writes known records that the Python decoder reads correctly.

## Milestone 7: add the asynchronous logger service

Create `logger.hpp/.cpp`. Start with namespace functions rather than exposing
queue or task handles:

```cpp
enum class SubmitResult { Accepted, InvalidRecord, QueueFull, Unavailable };
enum class State { NotStarted, Starting, Ready, Stopping, Stopped, Failed };

bool start() noexcept;                 // Before scheduler/producers; one shot.
SubmitResult submit(const Record& record) noexcept;
bool requestFlush() noexcept;          // Queued request, not completion.
bool requestStop() noexcept;           // Queued request, not completion.
Status status() noexcept;              // Consistent snapshot; task context.
```

Define `Status` with state, accepted/processed/synced/rejected counts, invalid
count, pending command count, current filename, first error code, and error
stage. Keep FatFs internals out of this public API. Define exact counter meanings
in the header: accepted means queued; processed includes RAM-buffered records;
synced means covered by successful filesystem sync, not universal power-loss
durability.

Keep a private `Command { Kind kind; Record record; }` in `logger.cpp`, where
Kind is Record/Flush/Stop. Assert the whole command is trivially copyable.
Use `xQueueCreateStatic()` because FreeRTOS
[copies queue items](https://www.freertos.org/media/2018/FreeRTOS_Reference_Manual_V10.0.0.pdf).
Use a static task control block, stack, queue storage, volume, and writer. Begin
with named configuration values: queue depth 64, sync interval 1000 ms, task stack
1024 `StackType_t` entries. These are starting values to measure, not guarantees.

Implement private `taskEntry(void*)`, `run()`, `handleCommand()`, `syncNow()`,
`beginShutdown()`, and `finishShutdown()`. The task entry is a static/free thunk;
do not cast a C++ member-function pointer into a FreeRTOS task callback.

Task startup uses the existing board binding, mounts, opens the log, then
publishes Ready. For the simplest initial contract, reject submissions until Ready. This
requires sensor tasks to wait for Ready and makes startup failure unambiguous.
If accepting during startup is needed later, add that behavior and tests.

Submission validates and copies a record with zero queue wait. Full queue rejects
the newest submission. Protect state validation, enqueue, and counters together
so they cannot race with stop. Do not hold a critical section during any I/O or
blocking queue operation. Status reads use the same protection.

Keep records and controls in the same FIFO. An accepted stop closes admission
atomically and drains earlier commands. Flush/stop requests can fail when full;
callers retry later from task context. Stop producers before requesting stop.

The worker receives with a timeout until the next sync deadline. Check the sync
deadline after each processed command too, so continuous data cannot postpone
sync forever. Sync failure immediately closes admission, then common cleanup
closes the file and unmounts. Preserve the first failure if cleanup also fails.
Publish Stopped only after successful drain/close/unmount; otherwise Failed.
With heap_1, leave the completed worker suspended/parked rather than deleting it.
Use an explicit parked state internally if suspending is selected; do not allow
an accidental resume to fall through the task entry.

Add ISR submission only if a real producer requires it. Use a dedicated
`submitFromIsr()` with the FreeRTOS wake flag, syscall-safe interrupt priorities,
and ISR critical sections; never call task-context submission from an ISR.

**Pass:** tests cover copied payload lifetime, full queue, command order, idle and
busy periodic sync, tick wrap, first-error preservation, rejection during error
cleanup, and successful stop. Only the logger task calls filesystem functions.

## Milestone 8: integrate a producer and measure

Make `app_init()` start the logger or the diagnostic build. Add one sensor/test
producer that waits for Ready, encodes a known value into a Record, and checks
SubmitResult. Confirm its data appears in decoded output before adding more
sources. Use explicit payload encoding, not casts to sensor structs.

Implement the application's shutdown flow: stop producers, retry requestStop
without busy-spinning, and wait for Stopped or Failed with an application timeout.
Only Stopped reports successful safe-removal completion.

Measure queue high-water use, rejected counts, maximum append/sync latency, and
task stack high-water use at the intended total record rate. An empty queue of
depth N covers roughly `N / records_per_second` seconds without consumption.
Size for measured storage stalls; average write throughput alone is insufficient.
Test absent/full cards, reboot and unique naming, and normal stop/removal.
Power-loss experiments should verify the decodable prefix without promising
that FAT metadata will always survive sudden removal.

If throughput is inadequate, optimize SPI block transfers and multi-block SD
commands as a later measured change. Leave DMA, automatic reinsertion/restart,
multiple filesystem clients, and new file formats outside the first release.

## How to assign the work

Assign one milestone per review. Ask the engineer to show the working check,
explain the error path, and describe which object/task owns each resource.
Do not accept "it compiles" as completion of a hardware or data-format milestone.
The first useful deliverable is a mounted-card text file; the second is a decoded
binary record; the third is a queued producer whose records survive orderly stop.
