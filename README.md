📥 Multi-Threaded File Downloader (C++17 + libcurl)

A lightweight and educational multi-threaded HTTP file downloader written in C++17, capable of downloading ultiple files simultaneously and large files in parallel by splitting them into byte-range chunks.
This project demonstrates practical systems-level concepts such as:

Multithreading with std::thread

HTTP Range requests via libcurl

Writing directly into specific byte offsets with pwrite()

Avoiding common pitfalls (vector reallocation, dangling references)

Building a local test environment using Python + Flask

🚀 Features

Parallel downloading using multiple threads

HTTP Range request support for fetching specific byte ranges

Shared file writing with no mutex required (each chunk writes to its own range)

Local test server included (miniServer.py)

Beginner-friendly, clean C++17 code

📂 Project Structure
Downloader/
 ├── Downloader.cpp            # Main entry point
 ├── DownloadBigFile.cpp       # Multi-threaded big-file downloading logic
 ├── DownloadMultipleFile.cpp  # (Optional) multiple-file downloader
 ├── helper.cpp / downloader.h     # Utilities: file size, callbacks, helpers
 ├── miniServer.py             # Flask test server for serving files
 └── README.md                 # Project documentation

🛠 Dependencies
System Requirements

Linux/macOS

C++17-compatible compiler (GCC/Clang)

POSIX API support (open, pwrite, ftruncate, etc.)

Libraries

libcurl

pthread (part of glibc)

Install libcurl on Ubuntu:

sudo apt-get install libcurl4-openssl-dev

For local testing

Install Flask:

pip install Flask

🔧 Build Instructions

Compile using g++ or build with CMake

🌐 Running the Local Test Server

Start the Flask server:

python3 miniServer.py


It exposes download endpoints:

http://localhost:19859/download/<filename>


Files are served from:

resources/


Example:

http://localhost:19859/download/historyBook.pdf

📥 Running the Downloader

Start the program:

./downloader


Input example:

http://localhost:19859/download/historyBook.pdf /home/.../destination/historyBook.pdf


The program will:

Get remote file size

Split file into N chunks

Launch N worker threads

Download using HTTP Range

Write each chunk into correct byte offset

Report progress

Sample output:

File size: 82100890
Progress: 10%
Progress: 25%
Progress: 40%
...
All chunks downloaded successfully.

🧠 How Chunk-Based Downloading Works

Each thread performs:

curl_easy_setopt(curl, CURLOPT_URL, url);
curl_easy_setopt(curl, CURLOPT_RANGE, "start-end");


And writes the result into the final file using:

pwrite(fd, buffer, bytes, offset);


Since chunks do not overlap, no locking is needed.

🪲 Common Pitfalls Solved in This Project
Issue	Cause	Fix
Dangling references	Passing ref(localChunk) to threads	Store chunks in vector<Chunk> and pass ref(chunk)
Vector reallocation	push_back() moves elements	Use reserve(num_threads)
Invalid URL errors	Uninitialized chunk objects	Proper chunk initialization
Partial content not supported	Server missing range support	Flask send_from_directory(..., conditional=True)
📘 Educational Value

This project is excellent for learning:

Safe multithreaded programming patterns

Correct lifetime & memory management in C++

Using curl's easy API for advanced HTTP features

POSIX system calls for file I/O

Debugging segmentation faults due to reallocation or dangling refs

🧩 Possible Future Extensions

Resume support (saving partial downloads)

Automatic retry logic

HTTPS/SSL support

Dynamic chunk sizing

CMake build system

GUI frontend (Qt, ImGui)

📜 License

This project is free to use for educational purposes.
You may modify and reuse the code in your own projects.
