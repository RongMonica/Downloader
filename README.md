# Modern Downloader

This refactor turns the original downloader into a small modern C++ project with clear layers:

- `DownloadManager`: schedules work and decides whether to use whole-file or range download.
- `ThreadPool`: fixed worker pool built with `std::jthread`.
- `HttpClient`: wraps libcurl operations.
- `ProgressReporter`: prints progress on a dedicated `std::jthread`.
- `FileWriter`: RAII wrapper for file descriptors.

## Modern C++ used

- `std::jthread`
- `std::stop_token`
- `std::future`
- `std::async`
- RAII wrappers
- `std::atomic`
- `std::shared_ptr`

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/modern_downloader
```

Then input:

```text
<url1> <output1> <url2> <output2>
```
