#include "downloader/download_manager.h"

#include <algorithm>
#include <future>
#include <thread>
#include <utility>

namespace downloader {

DownloadManager::DownloadManager(std::size_t worker_count) : pool_(worker_count) {}

DownloadStatePtr DownloadManager::add(DownloadRequest request) {
    auto state = std::make_shared<DownloadState>(std::move(request));
    states_.push_back(state);
    progress_.watch(state);
    return state;
}

std::vector<DownloadResult> DownloadManager::run_all() {
    progress_.start();

    std::vector<std::future<ProbeResult>> probe_futures;
    probe_futures.reserve(states_.size());
    for (const auto& state : states_) {
        state->status = DownloadStatus::Probing;
        probe_futures.push_back(std::async(std::launch::async, [this, url = state->request.url]() {
            return http_client_.probe(url);
        }));
    }

    std::vector<std::future<DownloadResult>> work_futures;
    work_futures.reserve(states_.size());

    for (std::size_t i = 0; i < states_.size(); ++i) {
        ProbeResult probe = probe_futures[i].get();
        const auto state = states_[i];
        if (!probe.ok) {
            work_futures.push_back(pool_.submit([state, probe]() {
                state->status = DownloadStatus::Failed;
                state->error_message = probe.error_message;
                return DownloadResult{state->request.url, state->request.output_path,
                                      DownloadStatus::Failed, 0, probe.error_message};
            }));
            continue;
        }

        if (probe.content_length > 0) {
            state->total_bytes = static_cast<std::uint64_t>(probe.content_length);
        }

        work_futures.push_back(pool_.submit([this, state, probe]() {
            return run_one(state, probe);
        }));
    }

    std::vector<DownloadResult> results;
    results.reserve(work_futures.size());
    for (auto& future : work_futures) {
        results.push_back(future.get());
    }

    progress_.stop();
    return results;
}

DownloadResult DownloadManager::run_one(const DownloadStatePtr& state, ProbeResult probe) {
    const bool can_split = probe.accept_ranges && probe.content_length > (1 << 20) &&
                           state->request.preferred_chunks > 1;

    if (can_split) {
        return http_client_.download_range_file(state, state->request.preferred_chunks, {});
    }
    return http_client_.download_whole_file(state, {});
}

}  // namespace downloader
