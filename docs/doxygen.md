# Application and BSP API

This reference covers `firmware/app` and `firmware/BSP`. The application owns
startup, storage, and logging; the BSP connects the SD driver to the board's
SPI bus and chip-select pin.

Currently, app_init() configures the SD interface through bsp::configureSd().
The storage and logging modules have host tests, but are not yet started by
the application or included in the CMake firmware target.

## Where to start

- app_init(): startup order and the C entry point into the C++ application.
- bsp::SdSpiPort: hardware requirements, byte transfers, chip select, and clock limits.
- storage_start_task(): the single task that owns FatFs and card access.
- logger_submit() and logger_submit_from_isr(): copy samples into the command queue.
- logger_get_status(): accepted, processed, and synced record counts.
- log_record_t: timestamps, source IDs, payload lengths, and encoding.
- logger_worker.h: the storage task's queue consumer and status updates.

## Generate and check the reference

Install [Doxygen](https://www.doxygen.nl/download.html) 1.9.8 or newer and put
`doxygen` on PATH. Graphviz is not required. From the repository root, run:

```sh
doxygen Doxyfile
```

Open `build/docs/html/index.html`. The configuration reports undocumented API
members, undocumented enum values, and invalid or incomplete parameter tags.
Warnings make the command fail. Generated files stay under the ignored `build/`
directory; only the source comments and configuration belong in version control.

To use CMake instead, run these commands from `firmware/` with the native host
toolchain described in `tests/README.md`:

```sh
cmake --preset host-tests -DTELEMETRY_BUILD_DOCS=ON
cmake --build --preset host-tests --target docs
```

This runs the same Doxyfile and writes to the same output directory. Documentation
is optional, so normal builds do not require Doxygen.

## Writing comments

Keep public contracts in the headers: what the caller supplies, which task may
call, what success means, and what happens on failure. Use source comments for
choices that need an explanation, such as preserving an earlier error or syncing
while the queue stays busy. Small helpers with clear names do not need a comment
for every step. When an API changes, update its header and the relevant README.
