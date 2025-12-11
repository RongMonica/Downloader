#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <curl/curl.h>
//#include <curl/easy.h>
#include <fcntl.h>  //open
#include <sys/stat.h> //mode constants
//#include <sys/types.h>
#include <unistd.h> //pwrite, ftruncate, close
//#include <errno.h>
//#include <stdlib.h>

#include <iostream>
#include <cstdio> //perror
#include <string>
#include <sstream>
#include <cstring>
#include <vector>
//#include <limits>
#include <thread>
#include <mutex>

using namespace std;
#define _XOPEN_SOURCE 700

class Task{
    public:
        string url;
        string outpath;
        int result; //0 = OK, else fail
        char errbuf[CURL_ERROR_SIZE];
        int last_percent;
};

class Progress{
    public:
        mutex mtx;
        curl_off_t total_size = 0;
        curl_off_t downloaded = 0;
        int last_percent = -1;
};

class Chunk{
    public:
        Progress* prog; //shared progress state
        string url; //URL to download
        int fd;            // output file descriptor (shared, safe with pwrite())
        curl_off_t start;   // inclusive
        curl_off_t end;     // inclusive
        int result; //0 = OK, else fail
};



int download_big_file(const vector<string>& vec_s);//return 0 on success, non-zero on fail
int download_multiple_files(const vector<string>& vec_s);//return 0 on success, non-zero on fail
static int progress_callback(void* clientp,
                             curl_off_t dltotal,
                             curl_off_t dlnow,
                             curl_off_t /*ultotal*/,
                             curl_off_t /*ulnow*/);
size_t write_data_file(void* buffer, size_t size, size_t nmemb, void *userp);
void download_one_file(Task& t); 

curl_off_t remote_file_size(const char* url);
size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata);
void download_one_chunk(Chunk& c); 





#endif // DOWNLOADER_H




