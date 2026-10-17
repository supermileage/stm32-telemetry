# SD and logging tests

Follow [host test setup](../README.md#run-the-tests), then run from `firmware/`:

```bash
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Build output goes to `build/host-tests` at the repository root. CMake includes
the decoder test only when Python and `tools/log_decoder.py` are available;
the script is currently missing.

To run only the SD/FatFs and callback tests from `firmware/`:

```bash
ctest --preset host-tests -R '^sd_(spi_and_fatfs|port_callbacks|spi_port_cpp)$'
```

## Coverage

- Portable C SD protocol plus the production C++ U5 BSP: SDHC/SDSC startup,
  addressing, transfers, and capacity.
- Invalid arguments, missing cards, bad responses, HAL errors, and timeouts.
- Callback validation, copied callback lifetime, and failure handling without HAL headers.
- Real FatFs on a simulated FAT16 card: Hello World write/read-back, distinct
  filenames, binary logs, mounting, and cleanup through the reusable
  `lib/drivers/sd_spi/fatfs` adapter.
- Logger queue overflow, ISR submissions, flush/stop ordering, sync timing,
  tick wraparound, and file errors.
- Standalone Hello World task success and failure paths; decoder CRC and
  incomplete-record checks.

The SD tests use a byte-level card model. The RTOS tests use a single-threaded
mock, so they do not check interrupt timing or real hardware. Run the
[storage board test](../../firmware/app/storage/README.md) before testing sensor loads.
