#include "downloader.h"

int download_multiple_files(const vector<string>& vec_s){
    vector<Task> tasks;
    for(int i = 0; i<vec_s.size();i += 2){
        string url = vec_s[i];
        string outpath = vec_s[i+1];
        tasks.emplace_back(url, outpath);
    }

    vector<thread> workers;
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

    return exit_code;
}
