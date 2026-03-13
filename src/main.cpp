#include "downloader/curl_raii.h"
#include "downloader/download_manager.h"

#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<std::string> split_tokens(const std::string& line) {
    std::stringstream ss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

}  // namespace

int main() {
    try {
        downloader::CurlGlobal curl_global;

        std::cout << "Input pairs: <url1> <output1> <url2> <output2> ...\n";
        std::string line;
        std::getline(std::cin, line);

        const auto tokens = split_tokens(line);
        if (tokens.size() < 2 || tokens.size() % 2 != 0) {
            std::cerr << "Expected URL/output pairs.\n";
            return 1;
        }

        const std::size_t worker_count = std::max<std::size_t>(2, std::thread::hardware_concurrency());
        downloader::DownloadManager manager(worker_count);

        for (std::size_t i = 0; i < tokens.size(); i += 2) {
            manager.add(downloader::DownloadRequest{tokens[i], tokens[i + 1], 4});
        }

        const auto results = manager.run_all();
        int exit_code = 0;
        for (const auto& result : results) {
            if (result.status == downloader::DownloadStatus::Completed) {
                std::cout << "Completed: " << result.url << " -> " << result.output_path << '\n';
            } else {
                std::cout << "Failed: " << result.url << " -> " << result.output_path
                          << " | " << result.error_message << '\n';
                exit_code = 1;
            }
        }
        return exit_code;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
}
