# Review notes

These issues were found during the style and documentation review. Their
implementations were left unchanged.

- In [`log_file_close()`](../firmware/app/logging/log_file.c), `log->open` is
  cleared even when `f_close()` fails. FatFs can leave the file object valid
  when its internal sync fails, so that flag can report a closed file while
  FatFs still has an open object. A later `log_file_open()` can then clear the
  object without completing the close. The failure is returned to the caller,
  but the writer's open-state bookkeeping needs a separate fix. The current
  close-error mock does not model FatFs retaining the file object.
- [`app_init()`](../firmware/app/app.cpp) configures the SPI binding but does
  not start storage. The CMake firmware target also omits the storage and
  logging sources. The `sd-hello` preset defines an option that startup does
  not read, so it currently runs the same startup as Debug.
- `tools/log_decoder.py` is missing. CMake omits the decoder test when the
  script is absent; passing the available host tests does not check decoding.
- The [IMU prototype](../SubProjects/B-U5-RTOS-IMU/README.md) now checks I2C
  transactions and decodes tagged FIFO samples. It retains the local GPIO-input
  setup and uses 100 ms polling; incoming interrupt support remains available
  if EXTI is configured later. Real interrupt timing, motion response, stack
  margins and long-running timestamp rollover still need on-board validation.

This repository has no application cache replacement policy to review. The
logging queue is FIFO and rejects the newest submission when full.
