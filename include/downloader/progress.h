#pragma once

#include "downloader/types.h"

#include <thread>
#include <memory>
#include <mutex>
#include <stop_token>
#include <vector>

namespace downloader {

class ProgressReporter {
public:
    ProgressReporter() = default;
    ~ProgressReporter();

    void watch(const DownloadStatePtr& state);
    void start();
    void stop();

private:
    void run(std::stop_token stop_token);

    std::mutex mutex_;
    std::vector<DownloadStatePtr> states_;
    std::unique_ptr<std::jthread> printer_;
};

}  // namespace downloader
