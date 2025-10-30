#define _XOPEN_SOURCE 700
#include <iostream>
#include <curl/curl.h>
#include <curl/easy.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string>

class Chunk{
    public:
        const char* url;
        int fd;            // output file descriptor (shared, safe with pwrite())
        long long start;   // inclusive
        long long end;     // inclusive
};

static long long g_total_size = 0;
static long long g_downloaded = 0;
static int g_last_percent = -1;
static pthread_mutex_t g_progress_mutex = PTHREAD_MUTEX_INITIALIZER;

// callback: libcurl calls this when it receives data
size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata){
    Chunk *part = (Chunk*)userdata;
    size_t total = size * nmemb;
    ssize_t written = pwrite(part->fd, ptr, total, part->start);
    if(written < 0){
        perror("pwrite");
        return 0;
    }
    part->start += written;

    if(g_total_size > 0){
        pthread_mutex_lock(&g_progress_mutex);
        g_downloaded += written;
        int percent = static_cast<int>((g_downloaded * 100) / g_total_size);
        if(percent > 100){
            percent = 100;
        }
        if(percent != g_last_percent){
            g_last_percent = percent;
            std::cout << "Progress: " << percent << "%" << std::endl;
        }
        pthread_mutex_unlock(&g_progress_mutex);
    }

    return static_cast<size_t>(written);
}

// Thread function
void *download_part(void *arg){
    Chunk *part = (Chunk*)arg;
    CURL* curl = curl_easy_init();
    if(!curl){
        std::cerr << "Failed to create CURL handle" << std::endl;
        return nullptr;
    }

    curl_easy_setopt(curl, CURLOPT_URL, part->url);

    // set the range
    char range[64];
    long long range_start = part->start;
    long long range_end = part->end;
    snprintf(range, sizeof(range), "%lld-%lld", range_start, range_end);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, part);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK){
        std::cerr << "Thread download failed: " << curl_easy_strerror(res) << std::endl;
    }
    curl_easy_cleanup(curl);
    return nullptr;
}

int main(int argc, char *argv[]){
    if(argc < 4){
        std::cerr << "Input not valid. Please input <function name> <url> <output> <num_threads>" << std::endl;
        return 1;
    }
    const char *url = argv[1];
    const char *outfile = argv[2];
    int num_threads = atoi(argv[3]);
    if(num_threads <= 0){
        num_threads = 1;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    // get the file size first
    CURL *curl = curl_easy_init();
    curl_off_t file_size = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // head request, no body
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    CURLcode res = curl_easy_perform(curl);
    if(res == CURLE_OK){
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &file_size);
    }else{
        std::cerr << "HEAD request failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);

    if(file_size <= 0){
        std::cerr << "cannot get file size. Maybe the server doesn't support it." << std::endl;
        curl_global_cleanup();
        return 1;
    }
    long long total_size = static_cast<long long>(file_size);
    g_total_size = total_size;
    g_downloaded = 0;
    g_last_percent = -1;

    if(num_threads > total_size && total_size > 0){
        num_threads = static_cast<int>(total_size);
    }
    if(num_threads <= 0){
        num_threads = 1;
    }

    std::cout << "File size: " << total_size << std::endl;

    int fd = open(outfile, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if(fd < 0){
        perror("open");
        curl_global_cleanup();
        return 1;
    }
    if(ftruncate(fd, (off_t)total_size) != 0){
        perror("ftruncate");
        close(fd);
        curl_global_cleanup();
        return 1;
    }

    long long part_size = total_size / num_threads;
    if(part_size <= 0){
        part_size = 1;
    }

    pthread_t threads[num_threads];
    Chunk parts[num_threads];

    for(int i = 0; i < num_threads; i++){
        parts[i].url = url;
        parts[i].fd = fd;
        parts[i].start = part_size * i;
        if(i == num_threads - 1){
            parts[i].end = total_size - 1;
        }else{
            parts[i].end = parts[i].start + part_size - 1;
        }
        pthread_create(&threads[i], NULL, download_part, &parts[i]);
    }

    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }
    if(g_total_size > 0 && g_last_percent < 100){
        std::cout << "Progress: 100%" << std::endl;
    }
    close(fd);
    curl_global_cleanup();
    std::cout << "Download completed: " << outfile << std::endl;
    return 0;
}
