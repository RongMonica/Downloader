#include "downloader.h"
#define DOWNLOAD_CHUNKS 5

//main entry: download one big file with multiple threads
//vec_s[0] = url, vec_s[1] = outpath
int download_big_file(const vector<string>& vec_s){
    CURLcode rc = curl_global_init(CURL_GLOBAL_ALL);
    if(rc != CURLE_OK){
        cerr << "curl_glocbal_init failed!" << endl;
        return 1;
    }

    string url = vec_s[0];
    string outpath = vec_s[1];

    //open the output file once and resize it
    int fd = open(outpath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    if(fd < 0){
        perror("open failed");
        return 1;
    }
    curl_off_t total_size = remote_file_size(url.c_str());
    cout << "File size: " << total_size << endl;
    if(total_size <= 0){
        cerr << "Cannnot get remore file size for URL: " << url << endl;
        curl_global_cleanup();
        close(fd);
        return 1;
    }
    //pre-allocate file on disk
    if(ftruncate(fd, total_size) != 0){
        perror("ftruncate failed.");
        curl_global_cleanup();
        close(fd);
        return 1;
    }

    //shared progress info
    Progress prog;
    prog.total_size = total_size;

    //cap threads to not exceed bytes
    int num_threads = DOWNLOAD_CHUNKS;
    if(num_threads > total_size){
        num_threads = static_cast<int>(total_size);
    }
    if(num_threads <= 0){
        num_threads = 1;
    }

    //split ranges
    const curl_off_t chunk_size = total_size / num_threads;
    const curl_off_t remainder = total_size % num_threads;
    
    vector<thread> workers;
    workers.reserve(num_threads);
    vector<Chunk> chunks;
    chunks.reserve(num_threads);
    curl_off_t offset = 0;

    for(int i = 0; i < num_threads; ++i){
        curl_off_t this_size = chunk_size + ((i == num_threads -1) ? remainder : 0);
        chunks.emplace_back();
        Chunk& chunk = chunks.back();
        chunk.prog = &prog;
        chunk.url = url;
        chunk.fd = fd;
        chunk.start = offset;
        chunk.end = offset + this_size - 1;
        chunk.result = 1;
        
        workers.emplace_back(download_one_chunk, ref(chunk));
        offset += this_size;
    }

    //wait for all threads
    for(auto& th : workers){
        if(th.joinable()){
            th.join();
        }
    }
    int exit_code = 0;
    int i = 0;
    for(auto& c : chunks){
        ++i;
        if(c.result != 0){
            cerr << "Chunk " << i << " fail to be downloaded" << endl;
            exit_code = 1;
        }
    }
    close(fd);
    curl_global_cleanup();
   
    return exit_code;
}
