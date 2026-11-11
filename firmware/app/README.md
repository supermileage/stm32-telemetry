# Application

Sensor tasks collect and timestamp measurements, then send them to the logger.
The logger saves one CSV per sensor on the SD card, such as
`0:/LOGS/RUN00001/GPS.CSV`. GPS is the only sensor in use so far.

## Files and folders

| File or folder | Purpose |
| --- | --- |
| `app.h`, `app.cpp` | Start the tasks |
| [sensors/](sensors/README.md) | Read sensors and prepare log records |
| [time/](time/README.md) | Read elapsed time and the GPS-initialized RTC |
| [logging/](logging/README.md) | Queue records and write CSV files |
| [storage/](storage/README.md) | Mount and unmount the SD card |

## Startup

`app_init()` creates the logger and GNSS tasks after peripheral setup and before
the scheduler starts. The logger then mounts the card and opens the files while
the GNSS task reads GPS data through USART3.

Only the logger task accesses the SD card. Other tasks send it records through
a queue. See the [logging guide](logging/README.md) for saving and stopping.

## Writing application code

Handwritten code in `app/` uses C++17 without exceptions or RTTI. Prefer plain
measurement structs, static storage, and small classes that manage their own
state. RTOS queues copy raw bytes, so their items must be trivially copyable.
Keep owning pointers, strings, and objects that need construction or destruction
out of queues.

Generated code, HAL, FreeRTOS, third-party libraries, and the BSP remain C.
`app_init()`, the sample clock, and the logger task API have C linkage so C code
can call them. Internal application APIs use C++ linkage.
