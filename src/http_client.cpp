#include "downloader/http_client.h"

#include "downloader/curl_raii.h"
#include "downloader/file_writer.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstring>
#include <future>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace downloader {

namespace {

struct HeaderParseContext {
    bool accept_ranges{false};
};

size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    const std::size_t total = size * nitems;
    auto* ctx = static_cast<HeaderParseContext*>(userdata);
    std::string header(buffer, total);
    std::string lower = header;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (lower.find("accept-ranges:") == 0 && lower.find("bytes") != std::string::npos) {
        ctx->accept_ranges = true;
    }
    return total;
}

std::size_t write_all_fd(int fd, const char* data, std::size_t total) {
    std::size_t written_total = 0;
    while (written_total < total) {
        const ssize_t written = ::write(fd, data + written_total, total - written_total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        written_total += static_cast<std::size_t>(written);
    }
    return written_total;
}

std::size_t pwrite_all_fd(int fd, const char* data, std::size_t total, std::int64_t offset) {
    std::size_t written_total = 0;
    while (written_total < total) {
        const ssize_t written = ::pwrite(fd, data + written_total, total - written_total,
                                         offset + static_cast<std::int64_t>(written_total));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        written_total += static_cast<std::size_t>(written);
    }
    return written_total;
}

std::int64_t compute_chunk_count(std::int64_t content_length, std::size_t preferred_chunks) {
    if (content_length <= 0) {
        return 1;
    }
    std::int64_t chunks = static_cast<std::int64_t>(std::max<std::size_t>(1, preferred_chunks));
    chunks = std::min<std::int64_t>(chunks, content_length);
    return std::max<std::int64_t>(1, chunks);
}

}  // namespace

ProbeResult HttpClient::probe(const std::string& url) const {
    ProbeResult result;
    try {
        auto handle = make_curl_handle();
        HeaderParseContext header_ctx;
        configure_common(handle.get(), url);
        curl_easy_setopt(handle.get(), CURLOPT_NOBODY, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(handle.get(), CURLOPT_HEADERDATA, &header_ctx);

        const CURLcode rc = curl_easy_perform(handle.get());
        if (rc != CURLE_OK) {
            result.error_message = curl_easy_strerror(rc);
            return result;
        }

        long http_status = 0;
        curl_off_t content_length = -1;
        curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &http_status);
        curl_easy_getinfo(handle.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);

        if (http_status >= 400) {
            result.error_message = "HTTP status " + std::to_string(http_status);
            return result;
        }

        result.ok = true;
        result.content_length = static_cast<std::int64_t>(content_length);
        result.accept_ranges = header_ctx.accept_ranges;
        return result;
    } catch (const std::exception& ex) {
        result.error_message = ex.what();
        return result;
    }
}

DownloadResult HttpClient::download_whole_file(const DownloadStatePtr& state,
                                               std::stop_token stop_token) const {
    state->status = DownloadStatus::Running;
    state->started_at = std::chrono::steady_clock::now();
    state->downloaded_bytes = 0;

    try {
        FileWriter writer(state->request.output_path, FileWriter::Mode::Truncate);
        auto handle = make_curl_handle();
        std::array<char, CURL_ERROR_SIZE> error_buffer{};
        StreamContext context{};
        context.state = &state;
        context.stop_token = stop_token;
        context.fd = writer.fd();

        configure_common(handle.get(), state->request.url);
        curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, &HttpClient::stream_write_callback);
        curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &context);
        curl_easy_setopt(handle.get(), CURLOPT_XFERINFOFUNCTION, &HttpClient::progress_callback);
        curl_easy_setopt(handle.get(), CURLOPT_XFERINFODATA, &context);
        curl_easy_setopt(handle.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(handle.get(), CURLOPT_ERRORBUFFER, error_buffer.data());

        const CURLcode rc = curl_easy_perform(handle.get());
        long http_status = 0;
        curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &http_status);
        state->http_status = http_status;

        if (stop_token.stop_requested() || rc == CURLE_ABORTED_BY_CALLBACK) {
            return cancelled_result(state);
        }
        if (rc != CURLE_OK) {
            const char* msg = error_buffer[0] != '\0' ? error_buffer.data() : curl_easy_strerror(rc);
            return failed_result(state, http_status, msg);
        }
        return success_result(state, http_status);
    } catch (const std::exception& ex) {
        return failed_result(state, 0, ex.what());
    }
}

DownloadResult HttpClient::download_range_file(const DownloadStatePtr& state,
                                               std::size_t chunk_count,
                                               std::stop_token stop_token) const {
    state->status = DownloadStatus::Running;
    state->started_at = std::chrono::steady_clock::now();
    state->downloaded_bytes = 0;

    try {
        const auto total_size = static_cast<std::int64_t>(state->total_bytes.load());
        FileWriter writer(state->request.output_path, FileWriter::Mode::ReadWriteTruncate);
        writer.resize(total_size);

        const std::int64_t chunks = compute_chunk_count(total_size, chunk_count);
        const std::int64_t base_chunk_size = total_size / chunks;
        const std::int64_t remainder = total_size % chunks;

        std::vector<std::future<DownloadResult>> futures;
        futures.reserve(static_cast<std::size_t>(chunks));
        std::int64_t offset = 0;

        for (std::int64_t i = 0; i < chunks; ++i) {
            const std::int64_t this_chunk_size = base_chunk_size + (i == chunks - 1 ? remainder : 0);
            const std::int64_t begin = offset;
            const std::int64_t end = begin + this_chunk_size - 1;
            offset = end + 1;

            futures.push_back(std::async(std::launch::async,
                [this, state, fd = writer.fd(), begin, end, stop_token]() {
                    try {
                        auto handle = make_curl_handle();
                        RangeContext context{};
                        context.state = &state;
                        context.stop_token = stop_token;
                        context.fd = fd;
                        context.next_offset = begin;
                        std::array<char, CURL_ERROR_SIZE> error_buffer{};

                        configure_common(handle.get(), state->request.url);
                        const std::string range = std::to_string(begin) + "-" + std::to_string(end);
                        curl_easy_setopt(handle.get(), CURLOPT_RANGE, range.c_str());
                        curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, &HttpClient::range_write_callback);
                        curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &context);
                        curl_easy_setopt(handle.get(), CURLOPT_XFERINFOFUNCTION, &HttpClient::progress_callback);
                        curl_easy_setopt(handle.get(), CURLOPT_XFERINFODATA, &context);
                        curl_easy_setopt(handle.get(), CURLOPT_NOPROGRESS, 0L);
                        curl_easy_setopt(handle.get(), CURLOPT_ERRORBUFFER, error_buffer.data());

                        const CURLcode rc = curl_easy_perform(handle.get());
                        long http_status = 0;
                        curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &http_status);

                        if (stop_token.stop_requested() || rc == CURLE_ABORTED_BY_CALLBACK) {
                            return cancelled_result(state);
                        }
                        if (rc != CURLE_OK) {
                            const char* msg = error_buffer[0] != '\0' ? error_buffer.data() : curl_easy_strerror(rc);
                            return failed_result(state, http_status, msg);
                        }
                        if (http_status != 206 && http_status != 200) {
                            return failed_result(state, http_status,
                                                 "range request returned unexpected HTTP status");
                        }
                        return success_result(state, http_status);
                    } catch (const std::exception& ex) {
                        return failed_result(state, 0, ex.what());
                    }
                }));
        }

        for (auto& future : futures) {
            DownloadResult result = future.get();
            if (result.status != DownloadStatus::Completed) {
                return result;
            }
        }

        return success_result(state, 206);
    } catch (const std::exception& ex) {
        return failed_result(state, 0, ex.what());
    }
}

std::size_t HttpClient::stream_write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* context = static_cast<StreamContext*>(userdata);
    if (context->stop_token.stop_requested()) {
        return 0;
    }

    const std::size_t total = size * nmemb;
    const std::size_t written = write_all_fd(context->fd, ptr, total);
    if (written == 0) {
        return 0;
    }

    if (context->state != nullptr && *context->state != nullptr) {
        (*context->state)->downloaded_bytes.fetch_add(written);
    }
    return written;
}

std::size_t HttpClient::range_write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* context = static_cast<RangeContext*>(userdata);
    if (context->stop_token.stop_requested()) {
        return 0;
    }

    const std::size_t total = size * nmemb;
    const std::size_t written = pwrite_all_fd(context->fd, ptr, total, context->next_offset);
    if (written == 0) {
        return 0;
    }

    context->next_offset += static_cast<std::int64_t>(written);
    if (context->state != nullptr && *context->state != nullptr) {
        (*context->state)->downloaded_bytes.fetch_add(written);
    }
    return written;
}

int HttpClient::progress_callback(void* clientp,
                                  curl_off_t dltotal,
                                  curl_off_t,
                                  curl_off_t,
                                  curl_off_t) {
    auto* context = static_cast<CallbackBase*>(clientp);
    if (context->stop_token.stop_requested()) {
        return 1;
    }
    if (context->state != nullptr && *context->state != nullptr && dltotal > 0) {
        auto& total_bytes = (*context->state)->total_bytes;
        if (total_bytes.load() == 0) {
            total_bytes = static_cast<std::uint64_t>(dltotal);
        }
    }
    return 0;
}

void HttpClient::configure_common(CURL* handle, const std::string& url) {
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "ModernDownloader/1.0");
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 0L);
}

DownloadResult HttpClient::cancelled_result(const DownloadStatePtr& state) {
    state->status = DownloadStatus::Cancelled;
    return DownloadResult{state->request.url, state->request.output_path, DownloadStatus::Cancelled,
                          state->http_status.load(), "cancelled"};
}

DownloadResult HttpClient::failed_result(const DownloadStatePtr& state, long http_status, std::string message) {
    state->status = DownloadStatus::Failed;
    state->http_status = http_status;
    state->error_message = message;
    return DownloadResult{state->request.url, state->request.output_path, DownloadStatus::Failed,
                          http_status, std::move(message)};
}

DownloadResult HttpClient::success_result(const DownloadStatePtr& state, long http_status) {
    state->status = DownloadStatus::Completed;
    state->http_status = http_status;
    return DownloadResult{state->request.url, state->request.output_path, DownloadStatus::Completed,
                          http_status, {}};
}

}  // namespace downloader
