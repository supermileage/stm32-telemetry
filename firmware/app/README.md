# Application code

This folder contains the sensor tasks, shared clock, and SD logger.

The firmware includes GPS reception and UTC synchronization. The logger still
writes binary files. The C++ `CsvFile` class is ready, but recording sessions and
sensor CSV files are the next step.

## Where to start

| Folder | What it does |
| --- | --- |
| `app.h`, `app.cpp` | Start the application tasks |
| `sensors/` | Read sensors and prepare records for the logger |
| `time/` | Provide elapsed time and GPS-based UTC |
| `logging/` | Queue records and write log files |
| `storage/` | Mount and unmount the SD card |

## Startup

`app_init()` runs after peripheral setup and before the scheduler starts. It
creates the logger and GNSS tasks. Once the scheduler runs, the logger mounts the
SD card and opens a file while the GNSS task starts receiving GPS data.

**Only the logger task accesses the SD card.** Sensor tasks send records through
the logger queue. Flush and stop requests use the same queue, so they are handled
after earlier records.

## Guides

- [Sensor records](sensors/README.md): add a sensor and submit its measurements.
- [GPS reception](sensors/gnss_logging.md): parse RMC messages and update UTC.
- [Shared clock](time/README.md): timestamp measurements when they are acquired.
- [Storage and logging](storage/storage_logging.md): check status, sync, and stop.
- [Binary format](logging/logging_format.md): read the current log files.
- [CSV file class](logging/csv_file.md): use the buffered C++ file writer.
