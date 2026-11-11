/** Buffer CSV rows and write them through FatFs from the logger task. */
#ifndef CSV_FILE_HPP
#define CSV_FILE_HPP

#include "ff.h"

#include <cstddef>

/** Own one file and a 4 KiB buffer. Keep instances in static storage. */
class CsvFile {
public:
    static constexpr std::size_t BufferSize = 4096U;

    constexpr CsvFile() = default;
    CsvFile(const CsvFile &) = delete;
    CsvFile &operator=(const CsvFile &) = delete;

    /**
     * Create a new file, write its header, and sync it. Existing files are kept.
     * A header write failure can leave the file open. Call close() during cleanup.
     */
    FRESULT open(const char *path, const char *header);

    /** Buffer one row of 1 to 4095 bytes without a line ending. Adds a newline. */
    FRESULT append(const char *row);

    /** Write buffered rows and sync the file's data and metadata. */
    FRESULT sync();

    /**
     * Flush and close the file, even after an earlier write or sync failure.
     * Returns the first error. After a close failure, stop using this writer
     * and unmount storage because FatFs may still hold an open handle.
     */
    FRESULT close();

    /** Whether the file still needs closing. */
    bool is_open() const { return open_; }

    /** Whether rows have been added since the last successful sync. */
    bool dirty() const { return dirty_; }

private:
    FRESULT flush();

    FIL file_{};
    char buffer_[BufferSize]{};
    UINT used_{};
    FRESULT error_{FR_OK};
    bool open_{};
    bool dirty_{};
};

#endif
