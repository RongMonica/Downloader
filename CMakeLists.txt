cmake_minimum_required(VERSION 3.15)

project(Downloader) 

add_executable(Downloader Downloader.cpp DownloadMultipleFile.cpp DownloadBigFile.cpp helper.cpp)

find_package(Threads REQUIRED)
target_link_libraries(Downloader PRIVATE Threads::Threads)

find_package(CURL REQUIRED)
target_link_libraries(Downloader PRIVATE CURL::libcurl)
