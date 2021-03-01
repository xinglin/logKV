#include <iostream>
#include <cassert> 
#include "Log.h"

int verboseLevel = 10;

using namespace std;
int main() {
    cout << "hello log KV" << endl;


    //LogLocator o(45, 100), o2(4, 50);
    //assert( o!=o2 );

    /*
    Logger l("./");
    string key ("key"), value ("myvalue"), key2("xxxxxx"), value2("I am number one!");
    LogLocator loc;
    for(int i=0;i<100000; i++) {
        l.append(key, value, 0xF1, 999, loc);
        //cout << "loc2: " << loc.fileno << ", " << loc.offset << endl;
    }
    */
    

    /*LogFile f(29, "./");
    KVRecord* r = f.load(60);
    cout << "keyLen=" << r->keyLen << ", valueLen=" << r->valueLen << endl;

    string activeFile = f.getActiveLogFileName();
    string file = f.getLogFileName();
    cout << "active file=" << activeFile << endl;

    if(rename(activeFile.c_str(), file.c_str()) != 0) {
        cerr << "rename active log failed for " << activeFile 
            <<  ", error is " << strerror(errno) << endl;
        exit(-1);
    }

    r = f.load(90);
    cout << "keyLen=" << r->keyLen << ", valueLen=" << r->valueLen << endl;
    */
    uint32_t total = 0, invalid = 0;
    double valid = (total-invalid)/(double)total;
    cout << "valid: " << valid << endl;
}
