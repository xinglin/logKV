#ifndef _FILE_OFFSET_H
#define _FILE_OFFSET_H
#include <string> 
#include "Common.h"
using std::string;



struct LogLocator {
    FileID         fileno;        // sequence id for a log file
    Offset          offset;         // offset in a log file. Max is 4 GB.

    LogLocator() : fileno(0), offset(0){};
    LogLocator(FileID fileno, Offset offset):
        fileno(fileno), offset(offset) {};

    bool operator==(const LogLocator& o2) const {
        return (fileno == o2.fileno && offset == o2.offset);
    }
    bool operator!=(const LogLocator& o2) const {
        return !(*this == o2);
    }
};



#endif