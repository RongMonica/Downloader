#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace downloader {

class FileWriter {
public:
    enum class Mode {
        Truncate,
        ReadWriteTruncate
    };

    FileWriter(const std::string& path, Mode mode);
    ~FileWriter();

    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;

    FileWriter(FileWriter&& other) noexcept;
    FileWriter& operator=(FileWriter&& other) noexcept;

    int fd() const { return fd_; }
    void resize(std::int64_t size) const;
    std::size_t write_all(const void* data, std::size_t size) const;
    std::size_t pwrite_all(const void* data, std::size_t size, std::int64_t offset) const;

private:
    int fd_{-1};
};

}  // namespace downloader
