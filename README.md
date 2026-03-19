# Modern Downloader

I built `Modern Downloader` as a small C++20 command line project that downloads one or more files from the internet using `libcurl`. My goal with this project was not just to make a downloader work, but to practice writing C++ code in a cleaner and more maintainable way.

I used this project to move away from a single-file style solution and refactor the downloader into smaller components with clear responsibilities. That gave me a chance to practice project structure, resource management, concurrency, and working with a C library through a C++ interface.

## Why I built this project

I wanted a project that would help me practice real software engineering skills in C++, not just syntax. In this project, I focused on:

- organizing code across multiple source and header files
- separating responsibilities across classes
- using RAII to manage resources safely
- using threads and asynchronous work to improve performance
- wrapping `libcurl` in a more C++-friendly design
- handling errors and reporting progress in a user-facing program

For me, this project was a good way to practice writing code that is easier to read, extend, and explain.

## How it works

At a high level, the program works like this:

1. It reads URL and output-file pairs from standard input.
2. It probes each URL first to check whether the file is reachable, what the content length is, and whether the server supports byte-range requests.
3. It decides whether to download the file as one full stream or split it into multiple chunks.
4. It schedules download tasks using a thread pool so multiple jobs can run concurrently.
5. It writes the data to disk and reports progress while downloads are running.
6. At the end, it prints whether each download completed successfully or failed.

If the file is large enough and the server supports range requests, the program downloads different parts of the file in parallel. Otherwise, it falls back to a normal whole-file download.

## Project structure

I split the project into a few small components:

- `DownloadManager`: manages the overall workflow, stores requests, probes each URL, chooses the download strategy, and collects the final results.
- `ThreadPool`: manages a fixed number of worker threads using `std::jthread`.
- `HttpClient`: handles the `libcurl` logic for probing URLs and downloading files.
- `ProgressReporter`: watches active downloads and prints progress updates from a separate thread.
- `FileWriter`: wraps file descriptor operations using RAII so files are handled safely.
- `CurlGlobal` and curl RAII helpers: handle `libcurl` setup and cleanup correctly.

I chose this structure because it makes the code easier for me to follow and makes each part of the program easier to reason about.

## Modern C++ features I used

This project gave me a chance to practice several modern C++ features:

- `std::jthread`
- `std::stop_token`
- `std::future`
- `std::async`
- RAII wrappers
- `std::atomic`
- `std::shared_ptr`

Using these features helped me write code that is safer and easier to manage, especially in the parts of the program that deal with threading and shared state.

## Build requirements

To build this project, I need:

- a C++20-compatible compiler
- CMake 3.20 or newer
- `libcurl`

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/modern_downloader
```

When the program starts, I enter URL/output-file pairs like this:

```text
https://example.com/file1.zip file1.zip https://example.com/file2.tar file2.tar
```

Each URL is followed by the output file path where the downloaded file should be saved.

## What I learned from this project

This project helped me practice:

- designing a small C++ project with better structure
- working with concurrency in a practical way
- managing low-level resources more safely
- turning a simple program into something more modular and maintainable

It also gave me a useful example project that I can keep improving over time.

## Possible future improvements

Some features I would like to add in the future are:

- unit tests for the non-networking logic
- command-line arguments instead of interactive input
- retry support for temporary network errors
- download cancellation from the console
- better logging or download statistics

## Summary

I see this project as a good example of how I am learning to write more modern and maintainable C++ code. It is a small project, but it helped me practice class design, concurrency, RAII, and integrating third-party libraries in a more structured way.
