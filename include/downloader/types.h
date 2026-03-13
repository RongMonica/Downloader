#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace downloader {

enum class DownloadStatus {
    Pending,
    Probing,
    Running,
    Completed,
    Failed,
    Cancelled
};

struct DownloadRequest {
    std::string url;
    std::string output_path;
    std::size_t preferred_chunks{4};
};

struct ProbeResult {
    bool ok{false};
    std::int64_t content_length{-1};
    bool accept_ranges{false};
    std::string error_message;
};

struct DownloadResult {
    std::string url;
    std::string output_path;
    DownloadStatus status{DownloadStatus::Failed};
    long http_status{0};
    std::string error_message;
};

struct DownloadState {
    explicit DownloadState(DownloadRequest req) : request(std::move(req)) {}

    DownloadRequest request;
    std::atomic<DownloadStatus> status{DownloadStatus::Pending};
    std::atomic<std::uint64_t> downloaded_bytes{0};
    std::atomic<std::uint64_t> total_bytes{0};
    std::atomic<long> http_status{0};
    std::string error_message;
    std::chrono::steady_clock::time_point started_at{};
};

using DownloadStatePtr = std::shared_ptr<DownloadState>;

inline const char* to_string(DownloadStatus status) {
    switch (status) {
        case DownloadStatus::Pending: return "Pending";
        case DownloadStatus::Probing: return "Probing";
        case DownloadStatus::Running: return "Running";
        case DownloadStatus::Completed: return "Completed";
        case DownloadStatus::Failed: return "Failed";
        case DownloadStatus::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

}  // namespace downloader
