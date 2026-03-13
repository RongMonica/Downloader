#pragma once

#include "downloader/types.h"

#include <curl/curl.h>

#include <cstddef>
#include <cstdint>
#include <stop_token>
#include <string>

namespace downloader {

class HttpClient {
public:
    ProbeResult probe(const std::string& url) const;

    DownloadResult download_whole_file(const DownloadStatePtr& state,
                                       std::stop_token stop_token) const;

    DownloadResult download_range_file(const DownloadStatePtr& state,
                                       std::size_t chunk_count,
                                       std::stop_token stop_token) const;

private:
    struct CallbackBase {
        const DownloadStatePtr* state{nullptr};
        std::stop_token stop_token{};
    };

    struct StreamContext : CallbackBase {
        int fd{-1};
    };

    struct RangeContext : CallbackBase {
        int fd{-1};
        std::int64_t next_offset{0};
    };

    static std::size_t stream_write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata);
    static std::size_t range_write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata);
    static int progress_callback(void* clientp,
                                 curl_off_t dltotal,
                                 curl_off_t dlnow,
                                 curl_off_t ultotal,
                                 curl_off_t ulnow);

    static void configure_common(CURL* handle, const std::string& url);
    static DownloadResult cancelled_result(const DownloadStatePtr& state);
    static DownloadResult failed_result(const DownloadStatePtr& state, long http_status, std::string message);
    static DownloadResult success_result(const DownloadStatePtr& state, long http_status);
};

}  // namespace downloader
