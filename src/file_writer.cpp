#include "downloader/file_writer.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

namespace downloader {

namespace {
std::runtime_error make_io_error(const std::string& prefix) {
    return std::runtime_error(prefix + ": " + std::strerror(errno));
}
}  // namespace

FileWriter::FileWriter(const std::string& path, Mode mode) {
    int flags = O_CREAT;
    if (mode == Mode::Truncate) {
        flags |= O_WRONLY | O_TRUNC;
    } else {
        flags |= O_RDWR | O_TRUNC;
    }

    fd_ = ::open(path.c_str(), flags, 0666);
    if (fd_ < 0) {
        throw make_io_error("open failed for " + path);
    }
}

FileWriter::~FileWriter() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

FileWriter::FileWriter(FileWriter&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

FileWriter& FileWriter::operator=(FileWriter&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void FileWriter::resize(std::int64_t size) const {
    if (::ftruncate(fd_, size) != 0) {
        throw make_io_error("ftruncate failed");
    }
}

std::size_t FileWriter::write_all(const void* data, std::size_t size) const {
    const auto* bytes = static_cast<const std::byte*>(data);
    std::size_t written_total = 0;

    while (written_total < size) {
        const ssize_t written = ::write(fd_, bytes + written_total, size - written_total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw make_io_error("write failed");
        }
        written_total += static_cast<std::size_t>(written);
    }
    return written_total;
}

std::size_t FileWriter::pwrite_all(const void* data, std::size_t size, std::int64_t offset) const {
    const auto* bytes = static_cast<const std::byte*>(data);
    std::size_t written_total = 0;

    while (written_total < size) {
        const ssize_t written = ::pwrite(fd_, bytes + written_total, size - written_total,
                                         offset + static_cast<std::int64_t>(written_total));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw make_io_error("pwrite failed");
        }
        written_total += static_cast<std::size_t>(written);
    }
    return written_total;
}

}  // namespace downloader
