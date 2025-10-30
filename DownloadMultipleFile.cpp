
#include <curl/curl.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>
#include <string>
#include <cstring>

class Task{
    public:
        const char* url;
        const char* outpath;
        int result; //0 = OK, else fail
        char errbuf[CURL_ERROR_SIZE];
};

 size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
 {
    // write to the FILE* passed via CURLOPT_WRITEDATA (stdout here)
    FILE* fp = static_cast<FILE*>(userp);
    size_t items = fwrite(buffer, size, nmemb, fp); 
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
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, t->errbuf);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L); // safer in multithreaded programs
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L); // treat HTTP errors as failures

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
        std::cerr << "Error: " << t->url << t->errbuf << std::endl;
    }else{
        t->result = 0;
    }

    curl_easy_cleanup(handle);
    fclose(fp);
    return nullptr;
}


int main(int argc, char* argv[]){
    if(argc !=3){
        std::cerr << "Usage: " << argv[0] << " <url> <outfile>" << std::endl;
        return 1;
    }
    curl_global_init(CURL_GLOBAL_ALL);

    Task task;
    task.url = argv[1];
    task.outpath = argv[2];
    task.result = 0;

    pthread_t tid;
    int err = pthread_create(&tid, NULL, download_thread, &task);
    if(err != 0){
        perror("pthread_create");
        curl_global_cleanup();
        return 1;
    }

    //wait for the single download to complete
    pthread_join(tid, NULL);

    curl_global_cleanup();

    if(task.result != 0){
        std::cerr << "Failed: " << task.url << "," << task.outpath << std::endl;
        return 1;
    }
    std::cout << "Task completed: " << task.url << "," << task.outpath << std::endl;

    return 0;
}
