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
        int fd;            //output file descriptor (shared, safe with pwrite())
        long long start;   //inclusive
        long long end;    //inclusive
};

//callback: libcur call this when it receives data
size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata){
    Chunk *part = (Chunk*)userdata;
    size_t total = size * nmemb;
    //write directly to the correct position in file
    pwrite(part->fd, ptr, total, part->start);
    part->start += total;
    return total;
}

//Thread function
void *download_part(void *arg){
    Chunk *part = (Chunk*)arg;
    CURL* curl = curl_easy_init();
    if(!curl){
        std::cerr << "Failed to create CURL handle" << std::endl;
        return nullptr;
    }
    //set URL
    curl_easy_setopt(curl, CURLOPT_URL, part->url);

    //set the range
    char range[64];
    sprintf(range, "%ld-%ld", part->start, part->end);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);

    //set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, part);

    //follow redirects if needed
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    //do the download
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
    if(num_threads <= 0) num_threads = 0;

    //init libcurl once
    curl_global_init(CURL_GLOBAL_ALL);

    //get the file size first;
    CURL *curl = curl_easy_init();
    double file_size = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // head request, no body
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    CURLcode res = curl_easy_perform(curl);
    if(res == CURLE_OK){
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &file_size);
    }else{
        std::cerr << "HEAD request failed: " << curl_easy_strerror(res) << std::endl;
    }
    
    curl_easy_cleanup(curl);

    if(file_size <=0){
        std::cerr << "cannot get file size. Maybe the server doesn't support it." << std::endl;
        curl_global_cleanup();
        return 1;
    }
    std::cout << "File size: " << file_size << std::endl;

    //open output file and set size
    int fd = open(outfile, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, (off_t)file_size); // pre-allocate space

    //split into ranges for threads
    long part_size = (long)(file_size/num_threads);
    pthread_t threads[num_threads];
    Chunk parts[num_threads];

    for(int i = 0; i < num_threads; i++){
        parts[i].url = url;
        parts[i].fd = fd;
        parts[i].start = i * part_size;
        if(i == num_threads -1){
            parts[i].end = (long)file_size -1;
        }else{
            parts[i].end = (i + 1) * part_size -1;
        }
        pthread_create(&threads[i], NULL, download_part, &parts[i]);
    }

    //wait for all threads to finish
    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }
    close(fd);
    curl_global_cleanup();
    std::cout << "Download completed: outfile" << std::endl;
    return 0;
}