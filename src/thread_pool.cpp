#include "downloader/thread_pool.h"

namespace downloader {

ThreadPool::ThreadPool(std::size_t worker_count) {
    if (worker_count == 0) {
        worker_count = 1;
    }
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this](std::stop_token stop_token) { worker_loop(stop_token); });
    }
}

ThreadPool::~ThreadPool() {
    request_stop();
}

void ThreadPool::request_stop() {
    {
        std::scoped_lock lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }

    for (auto& worker : workers_) {
        worker.request_stop();
    }
    cv_.notify_all();
}

void ThreadPool::worker_loop(std::stop_token stop_token) {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, stop_token, [this]() { return stopping_ || !jobs_.empty(); });
            if ((stopping_ && jobs_.empty()) || stop_token.stop_requested()) {
                return;
            }
            job = std::move(jobs_.front());
            jobs_.pop();
        }
        job();
    }
}

}  // namespace downloader
