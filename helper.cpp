#include "downloader.h"

// progress_callback() return the downloading progress of the "download_multiple_files()"
mutex progress_mtx;
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
            lock_guard<mutex> lock(progress_mtx);
            std::cout << "Downloading " << t->url << ": " << percent << "%" << endl;
        }
    }else if(dlnow != 0 && dlnow / (1024 * 1024) != t->last_percent){
        // Track roughly by megabytes when total size is unknown.
        t->last_percent = static_cast<int>(dlnow / (1024 * 1024));
        lock_guard<mutex> lock(progress_mtx);
        std::cout << "Downloading " << t->url << ": " << t->last_percent << " MiB received" << endl;
    }
    return 0;
}

 //write callback: write data into FILE*. Used inside download_one() of the download_multiple_files() project
 size_t write_data_file(void* buffer, size_t size, size_t nmemb, void *userp)
 {
    // write to the FILE* passed via CURLOPT_WRITEDATA (stdout here)
    FILE* fp = static_cast<FILE*>(userp);
    size_t items = fwrite(buffer, size, nmemb, fp); 
    if(items != nmemb){
        perror("Fwrite failed");
        return 0; // returning 0 tells libcurl to abort the transfer
    }
    return items * size; // MUST return bytes written
 }

// receive one task and download one file
 void download_one_file(Task& t){
    FILE* fp = fopen(t.outpath.c_str(), "wb");
    if(!fp){
        perror("fopen failed");
        return;
    }

    CURL* handle = curl_easy_init();
    if(!handle){
        fclose(fp);
        cerr << "curl_easy_init failed!" << endl;
        return;
    }

    curl_easy_setopt(handle, CURLOPT_URL, t.url.c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data_file);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp); // pass stdout to callback
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "DownloadMultipleFile/1.0");
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, t.errbuf);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L); // safer in multithreaded programs
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L); // treat HTTP errors as failures
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &t);
   
    t.errbuf[0] = '\0'; // ensure error buffer is null-terminated

    CURLcode rc = curl_easy_perform(handle);
    if(rc == CURLE_OK){
        long response_code = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
        if(response_code != 200){
            cerr << "Unexpected HTTP status " << response_code << " for " << t.url << endl;
            rc = CURLE_HTTP_RETURNED_ERROR;
        }
    }

    if(rc == CURLE_OK){
        char *content_type = nullptr;
        curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &content_type);
        if(content_type && strstr(content_type, "text/html") != nullptr){
            cerr << "Unexpected content type (" << content_type << ") for " << t.url << endl;
            rc = CURLE_RECV_ERROR;
        }
    }

    if(rc!=CURLE_OK){
        const char* err = t.errbuf[0] ? t.errbuf : curl_easy_strerror(rc);
        cerr << "Error downloading " << t.url << ": " << err << endl;
    }else{
        t.result = 0; // success
    }

    curl_easy_cleanup(handle);
    fclose(fp);
    return;
}

//BELOW ARE HELPER FUNCTIONS FOR DOWNLOAD BIG FILE FUNCTION
//assumes the server URL looks like http:://.../download/<filename>
//works becasue the file is on the same machine as the server. If the server were remote, use libcurl HEAD.
curl_off_t remote_file_size(const char* url){
    const char* marker = strstr(url, "/download/");
    if(!marker){
        return -1;
    }
    string rel(marker + 10); // skip "/download/"
    if(rel.empty()){
        return -1;
    }
    string full = "/home/r/Rong_coding/Downloader/resources/" + rel;
    struct stat st{};
    if(stat(full.c_str(), &st) != 0){
        return -1;
    }
    return st.st_size;
}

// callback: libcurl calls this when it receives data
size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata){
    Chunk *part = (Chunk*)userdata;
    size_t total = size * nmemb;

    //write exactly where this chunk should go
    ssize_t written = pwrite(part->fd, ptr, total, part->start);
    if(written < 0){
        perror("pwrite failed");
        return 0;
    }
    part->start += written; //advance this chunk's next write offset

    //progress (thread-safe)
    if(part->prog && part->prog->total_size > 0){
        lock_guard<mutex> lock(part->prog->mtx);
        part->prog->downloaded += written;
        int percent = static_cast<int>((part->prog->downloaded * 100) / part->prog->total_size);
        if(percent > 100){
            percent = 100;
        }
        if(percent != part->prog->last_percent){
            part->prog->last_percent = percent;
            cout << "Progress: " << percent << "%" << endl;
        }
    }
    return static_cast<size_t>(written); //return the number of bytes we consumed
}


// Thread function: download one range
void download_one_chunk(Chunk& c){
   
    CURL* handle = curl_easy_init();
    if(!handle){
        cerr << "curl_easy_init failed for chunk" << endl;
        return;
    }

    curl_easy_setopt(handle, CURLOPT_URL, c.url.c_str());
    // set the range
    string range = to_string(c.start) + "-" + to_string(c.end);
    curl_easy_setopt(handle, CURLOPT_RANGE, range.c_str());
    //write callback
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &c);

   // Misc options
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "DownloadBigFile/1.0");
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);

    CURLcode rc = curl_easy_perform(handle);
    if(rc != CURLE_OK){
        cerr << "Chunk download error: " << curl_easy_strerror(rc) << endl;
    }
    c.result = 0;
    curl_easy_cleanup(handle);
}