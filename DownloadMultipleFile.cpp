
#include <curl/curl.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

class Task{
    public:
        const char* url;
        const char* outpath;
        int result; //0 = OK, else fail
        char errbuf[CURL_ERROR_SIZE];
        int last_percent;
};

pthread_mutex_t progress_mutex = PTHREAD_MUTEX_INITIALIZER;

static int progress_callback(void* clientp,
                             curl_off_t dltotal,
                             curl_off_t dlnow,
                             curl_off_t /*ultotal*/,
                             curl_off_t /*ulnow*/){
    Task* t = static_cast<Task*>(clientp);
    if(!t){
        return 0;
    }

    if(dltotal > 0){
        int percent = static_cast<int>((dlnow * 100) / dltotal);
        if(percent > 100){
            percent = 100;
        }
        if(percent != t->last_percent){
            t->last_percent = percent;
            pthread_mutex_lock(&progress_mutex);
            std::cout << "Downloading " << t->url << ": " << percent << "%" << std::endl;
            pthread_mutex_unlock(&progress_mutex);
        }
    }else if(dlnow != 0 && dlnow / (1024 * 1024) != t->last_percent){
        // Track roughly by megabytes when total size is unknown.
        t->last_percent = static_cast<int>(dlnow / (1024 * 1024));
        pthread_mutex_lock(&progress_mutex);
        std::cout << "Downloading " << t->url << ": " << t->last_percent << " MiB received" << std::endl;
        pthread_mutex_unlock(&progress_mutex);
    }

    return 0;
}

 size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
 {
    // write to the FILE* passed via CURLOPT_WRITEDATA (stdout here)
    FILE* fp = static_cast<FILE*>(userp);
    size_t items = fwrite(buffer, size, nmemb, fp); 
    if(items != nmemb){
        std::perror("fwrite");
        return 0; // returning 0 tells libcurl to abort the transfer
    }
    return items * size; // MUST return bytes written
 }

void* download_thread(void* arg){
    Task* t = (Task*)arg;
    t->result = 1; //assue fail

    FILE* fp = fopen(t->outpath, "wb");
    if(!fp){
        perror("fopen");
        return nullptr;
    }

    CURL* handle = curl_easy_init();
    if(!handle){
        fclose(fp);
        std::cerr << "curl_easy_init failed!" << std::endl;
        return nullptr;
    }

    curl_easy_setopt(handle, CURLOPT_URL, t->url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp); // pass stdout to callback
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "DownloadMultipleFile/1.0");
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, t->errbuf);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L); // safer in multithreaded programs
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L); // treat HTTP errors as failures
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, t);
    t->errbuf[0] = '\0'; // ensure error buffer is null-terminated

    CURLcode rc = curl_easy_perform(handle);
    if(rc==CURLE_OK){
        long response_code = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
        if(response_code != 200){
            std::cerr << "Unexpected HTTP status " << response_code << " for " << t->url << std::endl;
            rc = CURLE_HTTP_RETURNED_ERROR;
        }
    }

    if(rc==CURLE_OK){
        char *content_type = nullptr;
        curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &content_type);
        if(content_type && std::strstr(content_type, "text/html") != nullptr){
            std::cerr << "Unexpected content type (" << content_type << ") for " << t->url << std::endl;
            rc = CURLE_RECV_ERROR;
        }
    }

    if(rc!=CURLE_OK){
        const char* err = t->errbuf[0] ? t->errbuf : curl_easy_strerror(rc);
        std::cerr << "Error downloading " << t->url << ": " << err << std::endl;
    }else{
        t->result = 0;
    }

    curl_easy_cleanup(handle);
    fclose(fp);
    return nullptr;
}


int main(int argc, char* argv[]){
    if(argc <3){
        std::cerr << "Usage: " << argv[0] << " <url> <outfile>" << std::endl;
        return 1;
    }
    if(((argc - 1) % 2) != 0){
        std::cerr << "Each URL needs a corresponding output file path." << std::endl;
        return 1;
    }

    CURLcode init_rc = curl_global_init(CURL_GLOBAL_ALL);
    if(init_rc != CURLE_OK){
        std::cerr << "curl_global_init failed: " << curl_easy_strerror(init_rc) << std::endl;
        return 1;
    }

    int num_threads = (argc-1)/2;
    std::vector<Task> tasks(num_threads);
    std::vector<pthread_t> tids(num_threads);
    for(int i = 0; i < num_threads; i++){
        tasks[i].url = argv[2*i+1];
        tasks[i].outpath = argv[2*i + 2];
        tasks[i].result = 0;
        tasks[i].errbuf[0] = '\0';
        tasks[i].last_percent = -1;
        int err = pthread_create(&tids[i], NULL, download_thread, &tasks[i]);
        if(err != 0){
            std::cerr << "pthread_create failed: " << std::strerror(err) << std::endl;
            for(int j = 0; j < i; ++j){
                pthread_join(tids[j], NULL);
            }
            curl_global_cleanup();
            return 1;
        }
    }
    
    //wait for the ALL downloads to complete
    int exit_code = 0;
    for(int i = 0; i < num_threads; i++){
        pthread_join(tids[i], NULL);
        if(tasks[i].result != 0){
            std::cerr << "Failed: " << tasks[i].url << "," << tasks[i].outpath << std::endl;
            exit_code = 1;
            continue;
        }
        std::cout << "Task completed: " << tasks[i].url << "," << tasks[i].outpath << std::endl;
    }

    curl_global_cleanup();

    return exit_code;
}
