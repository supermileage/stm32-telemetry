// Buffered CSV writer for the logger task.
#ifndef CSV_FILE_HPP
#define CSV_FILE_HPP

#include "ff.h"

#include <cstddef>

class CsvFile {
public:
    static constexpr std::size_t BufferSize = 4096U;

    constexpr CsvFile() = default;
    CsvFile(const CsvFile &) = delete;
    CsvFile &operator=(const CsvFile &) = delete;

    // Create a new file, write its header, and sync. Existing files are preserved.
    FRESULT open(const char *path, const char *header);

    // Buffer one row without a line ending. The writer appends a newline.
    FRESULT append(const char *row);

    // Write buffered rows and sync file data and metadata.
    FRESULT sync();

    // Flush and close from the logger task, even after an earlier error.
    // Returns the first error. If closing fails, stop using this writer and
    // unmount storage because FatFs may still consider the handle open.
    FRESULT close();

    bool is_open() const { return open_; }
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
