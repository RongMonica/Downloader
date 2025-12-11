#include "downloader.h"

int download_multiple_files(const vector<string>& vec_s){
    int num_tasks =  vec_s.size() / 2;
    vector<Task> tasks(num_tasks);
    for(int i = 0; i < num_tasks; i += 2){
        tasks[i].url = vec_s[i];
        tasks[i].outpath = vec_s[i + 1];
        tasks[i].result = 1;
        tasks[i].errbuf[0] = '\0';
        tasks[i].last_percent = -1;
        tasks.push_back(tasks[i]);
    }

    CURLcode init_rc = curl_global_init(CURL_GLOBAL_ALL);
    if(init_rc != CURLE_OK){
        cerr << "curl_global_init failed: " << curl_easy_strerror(init_rc) << endl;
        return 1;
    }

   
    vector<thread> workers;
    workers.reserve(num_tasks);
    for(auto& t : tasks){
        workers.emplace_back(download_one_file, ref(t));
    }
    
    //wait for the ALL downloads to complete
    for(auto& th : workers){
        if(th.joinable()){
            th.join();
        }
    }

    int exit_code = 0;
    for(auto& t : tasks){
        if(t.result != 0){
            cerr << "Failed: " << t.url << "," << t.outpath << endl;
            exit_code = 1;
        }else{
            cout << "Task completed: " << t.url << "," << t.outpath << endl;
        }
    }

    curl_global_cleanup();

    return exit_code;
}
