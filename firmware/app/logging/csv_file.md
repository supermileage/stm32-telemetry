# CSV file writer

`CsvFile` is a small C++ class for one CSV file. It owns the FatFs handle, a 4 KiB
buffer, and the saved error state. These members are private, and copying is
disabled so two objects cannot write through the same handle.

The class is tested, but the logger still uses the binary writer. Connecting CSV
files to recording sessions is the next stage.

## Use it from the logger task

Keep each `CsvFile` in static storage. Its buffer is too large for the logger's
current task stack. Only the logger task should call its file operations.

| Method | What it does |
| --- | --- |
| `open(path, header)` | Create a new file, write its header, and sync it |
| `append(row)` | Add a row to the buffer, writing the old buffer if space runs out |
| `sync()` | Write buffered rows and sync file data and metadata |
| `close()` | Write remaining rows and close the file |
| `is_open()` | Check whether a close attempt is still needed |
| `dirty()` | While open, check whether appended rows still need syncing |

Pass headers and rows without a line ending. They must contain 1 to 4,095 bytes;
`CsvFile` adds the newline. The caller formats the CSV fields and quotes text when
needed. Existing files are never overwritten.

A row that fits in the buffer causes no SD access. Writing a full buffer does not
sync the file. The logger will call `sync()` periodically when it starts using
this class.

## Check errors and close explicitly

Every file operation returns a FatFs result. The first write or sync error is
saved. A short write returns `FR_DENIED`, which can indicate a full card. Later
writes are blocked because retrying a partly written buffer could duplicate data.

Call `close()` after a successful open, including when the header sync fails.
It attempts cleanup even after an earlier error and preserves the first error.
If close fails, stop using the writer and unmount storage. FatFs may still hold a
valid handle, so reopening that writer is blocked.

The constructor and destructor do not access the SD card. Always close explicitly
so the logger can check the result before unmounting. The class uses no heap
allocation, exceptions, or RTTI.

A failed `f_open()` leaves no open handle, so another path can be tried. Choosing
paths, creating session folders, mounting storage, and timing syncs belong to the
logger. No CSV objects or their buffers are allocated by the current logger yet.
