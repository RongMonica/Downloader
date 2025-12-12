cmake_minimum_required(VERSION 3.15)

project(Downloader) 

add_executable(Downloader Downloader.cpp DownloadMultipleFile.cpp DownloadBigFile.cpp helper.cpp)

target_link_libraries(Downloader PRIVATE curl pthread)
