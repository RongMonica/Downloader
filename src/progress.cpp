#include "downloader/progress.h"

#include <chrono>
#include <iostream>

namespace downloader {

ProgressReporter::~ProgressReporter() {
    stop();
}

void ProgressReporter::watch(const DownloadStatePtr& state) {
    std::scoped_lock lock(mutex_);
    states_.push_back(state);
}

void ProgressReporter::start() {
    if (printer_) {
        return;
    }
    printer_ = std::make_unique<std::jthread>([this](std::stop_token stop_token) { run(stop_token); });
}

void ProgressReporter::stop() {
    if (printer_) {
        printer_->request_stop();
        printer_.reset();
    }
}

void ProgressReporter::run(std::stop_token stop_token) {
    using namespace std::chrono_literals;

    while (!stop_token.stop_requested()) {
        {
            std::scoped_lock lock(mutex_);
            for (const auto& state : states_) {
                const auto downloaded = state->downloaded_bytes.load();
                const auto total = state->total_bytes.load();
                const auto status = state->status.load();

                std::cout << '[' << to_string(status) << "] " << state->request.output_path << " : ";
                if (total > 0) {
                    const auto percent = static_cast<int>((downloaded * 100) / total);
                    std::cout << percent << "% (" << downloaded << '/' << total << " bytes)";
                } else {
                    std::cout << downloaded << " bytes";
                }
                std::cout << '\n';
            }
            if (!states_.empty()) {
                std::cout << "-----\n";
            }
        }
        std::this_thread::sleep_for(500ms);
    }
}

}  // namespace downloader
