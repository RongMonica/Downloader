cmake_minimum_required(VERSION 3.20)
project(modern_downloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_executable(modern_downloader
    src/main.cpp
    src/download_manager.cpp
    src/file_writer.cpp
    src/http_client.cpp
    src/progress.cpp
    src/thread_pool.cpp
)

target_include_directories(modern_downloader PRIVATE include)
target_link_libraries(modern_downloader PRIVATE CURL::libcurl)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(modern_downloader PRIVATE -Wall -Wextra -Wpedantic)
endif()
