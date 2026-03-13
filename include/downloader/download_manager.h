#pragma once

#include "downloader/http_client.h"
#include "downloader/progress.h"
#include "downloader/thread_pool.h"
#include "downloader/types.h"

#include <future>
#include <memory>
#include <vector>

namespace downloader {

class DownloadManager {
public:
    explicit DownloadManager(std::size_t worker_count);

    DownloadStatePtr add(DownloadRequest request);
    std::vector<DownloadResult> run_all();

private:
    DownloadResult run_one(const DownloadStatePtr& state, ProbeResult probe);

    ThreadPool pool_;
    HttpClient http_client_;
    ProgressReporter progress_;
    std::vector<DownloadStatePtr> states_;
};

}  // namespace downloader
