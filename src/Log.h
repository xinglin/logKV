#ifndef _LOG_FILE_H
#define _LOG_FILE_H

#include <string>
#include <cstring> // for strerror()
#include <atomic>
#include <fstream>
#include <iostream>
#include <vector>
#include <regex>
#include "Common.h"
#include "LogLocator.h"

extern const unsigned MAX_LOG_FILE_SIZE;
extern const std::string LOG_FILE_NAME;
extern const std::string LOG_FILE_POSTFIX;
extern const std::string ACTIVE_LOG_FILE;
extern int verboseLevel;
extern const std::regex LOG_REGEX;

/* on-disk format for a key/value pair. */
struct KVRecord
{
    CheckSum checksum;
    SequenceNumber sequenceNum;
    Flag flag;
    KeyLen keyLen;
    ValueLen valueLen;
    char *key;
    char *val;
};

struct FooterEntry
{
    KeyLen keyLen;
    string key;
    ValueLen valLen;
    Offset offset;
    SequenceNumber seqNum;
    Flag flag;

    FooterEntry(KeyLen keyLen, string key, ValueLen valLen, Offset offset, 
                SequenceNumber seqNum, Flag flag) : keyLen(keyLen), 
                valLen(valLen), key(key), offset(offset), seqNum(seqNum), 
                flag(flag) {}
};

class LogFile
{
    static std::atomic<FileID> nextFileID; // next file id.
    const string dir;
    FileID fileno;
    Offset offset;
    std::fstream log;
    std::vector<FooterEntry> entries;

public:
    static inline void setNextFileID(FileID fid)
    {
        nextFileID.store(fid);
    }
    static bool isLogFile(const string &path)
    {
        return std::regex_match(path, LOG_REGEX);
    }
    inline bool isDeleteLog() {
        return entries.size() > 0 && FlagDeleteBit(entries[0].flag);
    }

    LogFile(string dir);
    LogFile(FileID fileno, string dir);
    ~LogFile()
    {
        if (log.is_open())
        {
            close();
        }
    }

    void serializeFooter();
    void deserializeFooter();

    inline void close()
    {
        log.flush();
        log.close();
    }
    void closeRename();

    inline FileID getFileID() {
        return fileno;
    }
    
    inline std::string getLogFileName()
    {
        return dir + "/" + LOG_FILE_NAME + "_" + std::to_string(fileno) + LOG_FILE_POSTFIX;
    }

    inline std::vector<FooterEntry> &getFooterEntries()
    {
        return entries;
    }

    inline std::string getActiveLogFileName()
    {
        return dir + "/" + LOG_FILE_NAME + "_" + std::to_string(fileno) + ACTIVE_LOG_FILE + LOG_FILE_POSTFIX;
    }
    inline Offset getOffset()
    {
        return offset;
    }
    bool append(const std::string &key, const std::string &value,
                Flag flag, SequenceNumber seqNum, LogLocator &fo);

    KVRecord *load(Offset offset);
    void loadValue(Offset offset, string &_value);
};

/* A wrapper about LogFile. 
 * Its main function is to automatically start a new log file
 * when the current log file reaches the maximum size.
 * 
 * use Logger class only for writes. 
 */
class Logger
{
    std::string dir;
    std::string name;
    LogFile *log;

public:
    Logger(string dir, string name = "") : name(name), dir(dir)
    {
        log = new LogFile(dir);
    }
    ~Logger()
    {
        delete log;
    }
    void shutdown()
    {
        if (verboseLevel > 5)
            std::cout << name << " logger is shutting down" << std::endl;
        log->serializeFooter();
        log->closeRename();
    }

    // appendDelete calls append to do the actual append.
    bool appendDelete(const std::string &key, Flag flag, SequenceNumber seqNum);
    bool append(const std::string &key, const std::string &value,
                Flag flag, SequenceNumber seqNum, LogLocator &fo);
};

#endif