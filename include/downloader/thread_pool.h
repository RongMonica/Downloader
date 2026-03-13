#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <thread>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <vector>

namespace downloader {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Fn, typename... Args>
    auto submit(Fn&& fn, Args&&... args)
        -> std::future<std::invoke_result_t<Fn, Args...>> {
        using Result = std::invoke_result_t<Fn, Args...>;

        auto task = std::make_shared<std::packaged_task<Result()>>(
            std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
        std::future<Result> result = task->get_future();

        {
            std::scoped_lock lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            jobs_.push([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    void request_stop();

private:
    void worker_loop(std::stop_token stop_token);

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
    bool stopping_{false};
};

}  // namespace downloader
