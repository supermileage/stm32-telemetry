# Host tests

These unit and integration tests run on a PC without the board. They use a
simulated SD card and FreeRTOS mocks to exercise the driver, filesystem, and
logging code.

## Run the tests

Complete the [repository setup](../README.md#getting-started), then install
CMake 3.25+, Ninja, and native GCC (`gcc`) and put them on PATH. From the
repository root:

```bash
cd firmware
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Build output goes to `build/host-tests/` at the repository root. CTest prints
details for any failed tests.

## Coverage

| Folder | What it checks |
| --- | --- |
| [sd_spi/](sd_spi/README.md) | SD protocol, hardware callbacks, and real FatFs on a simulated card |
| `logging/` | Record queues, flush and stop ordering, file errors, and storage task startup and shutdown |

The mocks do not check real interrupt timing, task scheduling, or electrical
behavior. Follow the [storage board checks](../firmware/App/storage/README.md)
to verify the card and wiring on hardware.
