# Host tests

These unit and integration tests run on a PC without the board. They use a
simulated SD card, IMU register/FIFO model, and FreeRTOS mocks to exercise the driver, filesystem, and
logging code, plus the standalone IMU driver and task.

## Run the tests

Complete the [repository setup](../README.md#build-and-run), then install
CMake 3.25+, Ninja, and native GCC (`gcc` and `g++`) and put them on PATH. From the
repository root:

```bash
cd firmware
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Build output goes to `build/host-tests/` at the repository root. CTest prints
details for any failed tests.

The decoder test also needs Python and `tools/log_decoder.py`. That script is
missing from this checkout, so CMake currently omits the decoder test.

## Coverage

| Folder | What it checks |
| --- | --- |
| [sd_spi/](sd_spi/README.md) | SD protocol, C++ HAL callback integration/errors, and real FatFs on a simulated card |
| `imu/` | FIFO chunking, signed XYZ scaling, timestamps, bus errors, and EXTI/task notification/UART integration |
| `logging/` | Record queues, flush and stop ordering, file errors, and storage task startup and shutdown |

The mocks do not check real interrupt timing, task scheduling, or electrical
behavior. Follow the [storage board checks](../firmware/app/storage/README.md)
to verify the card and wiring on hardware.
