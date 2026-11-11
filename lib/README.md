# External Libraries

Libraries are maintained as Git submodules, pinned to specific revisions. Keep upstream sources unchanged. Project configuration and hardware interfaces belong in `firmware/`.

Initialize from the repository root:

```sh
git submodule update --init --recursive
```

STM32CubeIDE links the libraries through `Lib`. Only the sources listed below are compiled in Debug and Release. Examples, tests, bundled firmware, and optional drivers are excluded.

## Libraries

**FatFs (R0.12c)**
- Sources: `src/ff.c`, `src/diskio.c`, `src/ff_gen_drv.c`
- Config: `firmware/config/fatfs/ffconf.h`
- SD interface: `firmware/bsp/storage/sd_diskio.cpp`
- One writable volume, 512-byte sectors, short filenames (8.3), FAT12/16/32.
- exFAT, long filenames, formatting, and RTC timestamps disabled.
- Only the storage task may access the filesystem.

**Minmea**
- Source: `minmea.c`
- Used by GNSS code in `firmware/app/sensors`.
- Compatibility header: `firmware/config/minmea`
- Uses the documented `mktime` fallback because Newlib does not provide `timegm`. The C runtime timezone must remain UTC.

**SD SPI Driver**

[STM32_HAL-SPI_SD-FatFs](https://github.com/akasa828/STM32_HAL-SPI_SD-FatFs)

- Pinned revision: `4a0c55f861fc9dc01889e49a66e132d2562f9820`
- Source: `Core/Micro_SD/SD_reader.c`
- HAL interface: `firmware/bsp/storage/sd_spi_port.cpp`
- Only the portable protocol core is used. Do not compile its bundled FatFs or STM32F1 HAL adapter.
- SPI callbacks use blocking transfers with finite timeouts, trailing clocks with CS high, and data CRC checking.
- Both speed modes retain the CubeMX SPI clock setting of 250 kHz.
- `SD_ENABLE_SELF_TEST=0` disables the raw-sector self-test.
- No Arduino runtime dependency or duplicated local protocol code.

## Tests

Run the SD library's host tests from the repository root:

```sh
cmake -S lib/sd_spi/tests -B build/sd-library-tests
cmake --build build/sd-library-tests
ctest --test-dir build/sd-library-tests --output-on-failure
```

Tests cover protocol commands, CRC, addressing, and timeouts.

After updating a library, review its API and configuration requirements, run the tests, and build both Debug and Release. Verify SD card write/read-back on the target board before using a new driver for telemetry logging.