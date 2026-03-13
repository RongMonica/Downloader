#pragma once

#include <curl/curl.h>

#include <memory>
#include <stdexcept>

namespace downloader {

class CurlGlobal {
public:
    CurlGlobal() {
        const CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (rc != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(rc));
        }
    }

    ~CurlGlobal() { curl_global_cleanup(); }

    CurlGlobal(const CurlGlobal&) = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;
};

struct CurlEasyDeleter {
    void operator()(CURL* handle) const {
        if (handle != nullptr) {
            curl_easy_cleanup(handle);
        }
    }
};

using CurlHandle = std::unique_ptr<CURL, CurlEasyDeleter>;

inline CurlHandle make_curl_handle() {
    CurlHandle handle{curl_easy_init()};
    if (!handle) {
        throw std::runtime_error("curl_easy_init failed");
    }
    return handle;
}

}  // namespace downloader
