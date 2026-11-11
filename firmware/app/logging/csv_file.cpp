#include "csv_file.hpp"

#include <algorithm>
#include <cstring>

FRESULT CsvFile::open(const char *path, const char *header)
{
    if (open_) {
        return FR_INVALID_OBJECT;
    }
    if (error_ != FR_OK) {
        return error_;
    }
    if (path == nullptr || header == nullptr || header[0] == '\0' ||
        std::strlen(header) >= BufferSize) {
        return FR_INVALID_PARAMETER;
    }

    FRESULT result = f_open(&file_, path, FA_WRITE | FA_CREATE_NEW);
    if (result != FR_OK) {
        return result;
    }
    open_ = true;
    used_ = 0U;

    result = append(header);
    return result == FR_OK ? sync() : result;
}

FRESULT CsvFile::append(const char *row)
{
    if (!open_) {
        return FR_INVALID_OBJECT;
    }
    if (error_ != FR_OK) {
        return error_;
    }
    if (row == nullptr) {
        return FR_INVALID_PARAMETER;
    }

    std::size_t length = std::strlen(row);
    if (length == 0U || length >= BufferSize) {
        return FR_INVALID_PARAMETER;
    }
    if (used_ + length + 1U > BufferSize && flush() != FR_OK) {
        return error_;
    }

    std::copy_n(row, length, buffer_ + used_);
    used_ += static_cast<UINT>(length);
    buffer_[used_++] = '\n';
    dirty_ = true;
    return FR_OK;
}

FRESULT CsvFile::flush()
{
    if (error_ != FR_OK || used_ == 0U) {
        return error_;
    }

    UINT written = 0U;
    error_ = f_write(&file_, buffer_, used_, &written);
    if (error_ == FR_OK && written != used_) {
        /** FatFs can return success after a partial write when the card is full. */
        error_ = FR_DENIED;
    }
    if (error_ == FR_OK) {
        used_ = 0U;
    }

    return error_;
}

FRESULT CsvFile::sync()
{
    if (!open_) {
        return FR_INVALID_OBJECT;
    }

    if (flush() == FR_OK) {
        error_ = f_sync(&file_);
        if (error_ == FR_OK) {
            dirty_ = false;
        }
    }

    return error_;
}

FRESULT CsvFile::close()
{
    if (!open_) {
        return error_;
    }

    FRESULT result = flush();
    FRESULT closed = f_close(&file_);
    open_ = false;
    dirty_ = false;
    error_ = result == FR_OK ? closed : result;
    return error_;
}
