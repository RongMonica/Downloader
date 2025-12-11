#include "downloader.h"


int main(){
    cout << "Please input addresses (separate by spaces) for your files to be downloaded and the target file paths: " << endl;
    cout << "Example format: <url1> <outfile1> <url2> <outfile2>" << endl; 

    string url_address, temp;
    vector<string> url_address_vec;
    getline(cin, url_address);
    if(url_address.size() == 0) {
        cerr << "Invalid input. Please run the program again!" << endl;
        return 1;
    }
    stringstream ss(url_address);
    while(ss >> temp){
        url_address_vec.push_back(temp);
    }
    int size = url_address_vec.size();
    if(size < 2 || size % 2 != 0){
        cerr << "Each URL nees a corresponding output file path. Please fun the program again!" << endl;    
    }else if (size == 2){
        download_big_file(url_address_vec);
    }else{
        download_multiple_files(url_address_vec);
    }
    return 0;
}