# Local setup and incoming IMU merge

The merge combines local `main` (checkpoint `bb755a8`, including the 59 staged
files present before this task) with `origin/main` at `69d0e7d`. The original
local head was `c8f81c9`. No remote push is part of this work; existing stashes
are preserved. A binary patch of the original index is also saved locally at
`build/merge-safety/original-staged.patch`.

## Resolutions

| Conflict or incompatible incoming change | Resolution |
| --- | --- |
| IMU `Core/Src/main.c` | Retained local generated initialization, UART4, task priorities, clocks, and peripheral setup. Replaced the old IMU placeholder with the incoming FIFO task. Checked task creation and suspended startup instead of deleting a task with `heap_1`. |
| IMU `README.md` | Combined setup/build guidance and useful incoming sensor/FIFO notes; updated behavior and validation limits. |
| Board schematic and IMU application note added under the old document directory | Retained both PDFs under the local `docs/datasheets/` layout. |
| Incoming CubeMX regeneration and `heap_4` switch | Retained the saved local `.ioc`, `.mxproject`, FreeRTOS configuration, HAL and middleware versions, and generated peripheral initialization. |
| IMU CubeIDE source paths | Retained local build settings and added only `App` / `App/drivers` include paths and the `App` source entry for Debug and Release. |
| Incoming EXTI11 support versus local PE11 GPIO-input setup | Preserved the pin/NVIC settings. Kept the IRQ handler, callback, and task notifications; the task uses its 100 ms polling fallback with the current setup. |

The SD driver, FatFs, board bindings, storage, logging modules, and main
firmware configuration are unchanged from the local checkpoint. The IMU
remains in `SubProjects/B-U5-RTOS-IMU`; it does not submit records to the SD
logger or start from the main firmware.

## IMU repairs

The incoming FIFO code referenced undefined UART buffers and sample arrays,
discarded decoded values, and ignored bus errors. The merged implementation
retains its FIFO/timestamp approach and adds accessible latest readings,
sample counts, overrun/unknown-tag counts, and clock pairs. It validates the
device identity, checks all register transactions, retries failed startup,
and never decodes a failed read. Draining is bounded; UART4 output is limited
to the latest values at 10 Hz so UART cannot stall every FIFO sample.

Register interpretation was checked against ST's
[register definitions](https://github.com/STMicroelectronics/ism330dhcx-pid/blob/master/ism330dhcx_reg.h)
and [FIFO application note](https://www.st.com/resource/en/application_note/an5398-ism330dhcx-alwayson-3d-accelerometer-and-3d-gyroscope-with-digital-output-for-industrial-applications-stmicroelectronics.pdf).

## Validation

- CMake/GNU Arm main firmware: Debug and Release built successfully.
- STM32CubeIDE main firmware: Debug and Release built, zero errors/warnings.
- STM32CubeIDE standalone IMU: Debug and Release built, zero errors/warnings.
- Host suite: all 25 tests passed (22 SD/logging tests plus three IMU scenarios).
  IMU coverage includes chunk and FIFO-count boundaries, signed scale extremes,
  timestamp decoding, each initialization transaction failing, read failures,
  a continuously full FIFO, wrong-device rejection, and EXTI/task/UART integration.
- The linked IMU ELF contains strong implementations of `IMU_task`,
  `EXTI11_IRQHandler`, and `HAL_GPIO_EXTI_Rising_Callback`.
- The configuration audit checks local CubeMX/FreeRTOS files and generated
  initialization against the checkpoint, allowing only the documented user-code
  hooks and additive IMU build paths.

Build logs and host test results are under `build/merge-safety/`,
`build/merge-check-host/Testing/Temporary/`, and `build/merge-check-{debug,release}/`.

## Remaining issues

No board was flashed or exercised during this merge. Motion response, bus and
interrupt timing, task stack margins, and sustained FIFO behavior still need
the [on-board checks](../SubProjects/B-U5-RTOS-IMU/README.md). PE11 interrupts
remain disabled to preserve the requested setup. Intermediate IMU samples are
decoded but not persisted; calibration, asynchronous I2C, and long-running
32-bit timestamp rollover handling remain future work.

Pre-existing SD limitations remain documented in [review notes](review_notes.md):
storage/logger startup is not enabled in the main CMake firmware, `sd-hello`
has no startup implementation, and the missing `tools/log_decoder.py` prevents
the decoder test from running. The existing close-error bookkeeping issue also
remains. These were present in the local checkpoint and are separate from IMU
integration.
