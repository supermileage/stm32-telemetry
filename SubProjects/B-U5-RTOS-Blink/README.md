Project to learn very basics of RTOS, just to blink an LED

## Notes
- Green user LED on pin PH7 accoridng to the manual, confirmed it was initialized in cubemx
- Using `vTaskDelay` for non blocking delay
- 500 / portTICK_PERIOD_MS converts the number into the right value to be interpretted as milliseconds based on the interval the scheduler runs from rtos config
- STM has wrappers for RTOS code to make the code portable to any RTOS, this is CMSIS. It is unlikely we will need to port, sticking to the standard gives more features and is faster, learning FreeRTOS is generally more useful so it would make sense to stick to them. In general functions that start with `os` are wrappers and ones that start with `x` or `v` are from FreeRTOS.
- Task functions should never return, they can delete themselves when finished or usually just loop forever
- Functions prefixed with `x` = return a value, `v` = void
- "Variables of non stdint types are prefixed x." [freertos style guide](https://freertos.org/Documentation/02-Kernel/06-Coding-guidelines/02-FreeRTOS-Coding-Standard-and-Style-Guide)
- Getting task handle during task creating can allow other tasks to suspend, delete or perform actions on it
