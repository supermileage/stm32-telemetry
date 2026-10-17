# FreeRTOS LED blink

A small project for learning FreeRTOS on the B-U585I-IOT02A. The `toggleLed`
task toggles the green user LED on PH7, then waits 500 ms using
`vTaskDelay(500 / portTICK_PERIOD_MS)`.

Import this directory into STM32CubeIDE, build it, connect the board through
ST-LINK, and select **Run As** or **Debug As > STM32 C/C++ Application**.

`vTaskDelay()` blocks only the calling task, so the scheduler can run other
tasks. Its argument is in ticks; `portTICK_PERIOD_MS` converts this delay
from milliseconds for the configured tick rate.

The project uses native FreeRTOS task calls alongside the generated
CMSIS-RTOS setup. CMSIS task functions usually begin with `os`; native
FreeRTOS functions include `xTaskCreate()` and `vTaskDelay()`. FreeRTOS task
functions must not return. This blink task loops forever. Whether a task can
delete itself depends on the selected allocator; the main firmware's
`heap_1` does not support deletion.

The task handle saved by `xTaskCreate()` lets other tasks refer to this task,
for example to suspend it. See the
[FreeRTOS coding style guide](https://freertos.org/Documentation/02-Kernel/06-Coding-guidelines/02-FreeRTOS-Coding-Standard-and-Style-Guide)
for the API naming conventions.
