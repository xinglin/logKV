#include "Log.h"
#include <cassert>
#include <cstdio>   // for file rename
#include <cstdint>
#include <filesystem>   // for check file exist.
#include <memory>       // for shared_ptr

using std::string;
using std::ofstream;
using std::cout;
using std::endl;
using std::cerr;
namespace fs = std::filesystem;

const unsigned MAX_LOG_FILE_SIZE = 100*1024;       // 100 MiB
const string LOG_FILE_NAME = "log";
const string LOG_FILE_POSTFIX = ".kv";                  // closed/sealed log files.
const string ACTIVE_LOG_FILE = "_active";       // log files that are open and receiving more writes.
const string LOG_FILE_FORMAT = LOG_FILE_NAME + "\\_" + "([0-9]+)\\"+LOG_FILE_POSTFIX;
const std::regex LOG_REGEX (LOG_FILE_FORMAT);


const int KVRecordMetaDataLen = sizeof(CheckSum)+sizeof(SequenceNumber) 
    + sizeof(Flag)+sizeof(KeyLen)+sizeof(ValueLen);

std::atomic<FileID> LogFile::nextFileID(0);

/* Use this method to create a new log file for writes. */
LogFile::LogFile(string dir): dir(dir) {
    // if dir is not set, set it to current path
    if(dir.empty())
        dir = fs::current_path();

    fileno = nextFileID.fetch_add(1);
    offset = 0;
    string filename = getActiveLogFileName();
    if(verboseLevel >= 5)
        cout << "logfile = " << filename << endl;
    log.open(filename, std::ios_base::out|std::ios_base::binary);
    if(!log)
        cerr << "failed to open log file: " << filename 
            << ", error: " << strerror(errno) << endl;
}

/* Use this method to open a log file for reads. */
LogFile::LogFile(FileID fileno, string dir): dir(dir) {

    // if dir is not set, set it to current path
    if(dir.empty())
        dir = fs::current_path();

    this->fileno = fileno;
    string name = getLogFileName();
    if(!fs::exists(name)) {
        // try active log, 
        name = getActiveLogFileName();

        if(!fs::exists(name)){
            cerr << "log file " << fileno << " does not exist" << endl;
            exit(1);
        }
    }
    log.open(name, std::ios_base::in|std::ios_base::binary);
    if(!log)
        cerr << "failed to open log file: " << name 
            << ", error: " << strerror(errno) << endl;
}

/*
 * LogLocator& fo: is output
 */
bool LogFile::append(const string& key, const string& value, 
                        Flag flag, SequenceNumber seqNum, LogLocator& fo)
{
    CheckSum checksum = 100;
    KeyLen keyLen = key.size();
    ValueLen valueLen = value.size();
    log.write(reinterpret_cast<char*>(&checksum), sizeof(checksum));
    log.write(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
    log.write(reinterpret_cast<char*>(&flag), sizeof(flag));
    log.write(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
    log.write(reinterpret_cast<char*>(&valueLen), sizeof(valueLen));

    log.write(key.c_str(), keyLen);
    log.write(value.c_str(), valueLen);

    log.flush();
    fo.fileno = fileno;
    fo.offset  = offset;

    entries.emplace_back(keyLen, key, valueLen, offset, seqNum, flag);
    offset += (KVRecordMetaDataLen + keyLen + valueLen);
    return true;
}

// get the value for a key/value pair. 
void LogFile::loadValue(Offset offset, string& _value) {
    KVRecord * r = load(offset);
    // TODO: this creates a copy of value.
    _value.assign(r->val, r->valueLen);
    delete r;  
}

// load a complete KV record
KVRecord* LogFile::load(Offset offset) {
    KVRecord* r = nullptr;
    assert(log);
    log.seekg(offset, std::ios_base::beg);
    if(!log) {
        cerr << "seek error when loading for log file: " << getLogFileName() 
            << ", error is " << strerror(errno) << endl;
        return nullptr; 
    }

    r = new KVRecord();
    log.read((char*)&r->checksum, sizeof(CheckSum));
    log.read((char*)&r->sequenceNum, sizeof(SequenceNumber));
    log.read((char*)&r->flag, sizeof(Flag));
    log.read((char*)&r->keyLen, sizeof(KeyLen));
    log.read((char*)&r->valueLen, sizeof(ValueLen));
    assert(r->keyLen > 0);
    assert(r->valueLen  > 0);
    r->key = (char *)malloc(r->keyLen);
    r->val = (char *)malloc(r->valueLen);

    log.read(r->key, r->keyLen);
    log.read(r->val, r->valueLen);
    return r;
}

// serialize the footer section for this log file
void LogFile::serializeFooter() {
    assert(log.is_open());

    Offset footerOffset = log.tellp();
    uint32_t num = entries.size();
    for(auto & e: entries) {
        log.write((char*) &e.keyLen, sizeof(KeyLen));
        log.write(e.key.c_str(), e.keyLen);
        log.write((char*) &e.valLen, sizeof(ValueLen));
        log.write((char*) &e.offset, sizeof(Offset));
        log.write((char*) &e.seqNum, sizeof(SequenceNumber));
        log.write((char*) &e.flag, sizeof(Flag));
    }

    CheckSum checksum {0};
    log.write((char*) &footerOffset, sizeof(Offset));
    log.write((char*) &num, sizeof(num));
    log.write((char*) &checksum, sizeof(CheckSum));
}

/* 
 * Deserialize the footer section for this log file
 * 
 * Read the footer section of a log file and populate the
 * vector<FooterEntry> entries. 
 * Used later during cold startup to build the in-memory 
 * hashmap index.  
 */ 
void LogFile::deserializeFooter() {
    assert(log.is_open());

    // entries vector should be empty.
    assert(entries.empty());

    Offset offset;
    uint32_t num;
    CheckSum checksum;
    int offsetFromEnd = sizeof(offset) + sizeof(num) + sizeof(checksum);

    // negative offset relative to the end
    offsetFromEnd = -offsetFromEnd;

    // read the three metadata fields from the end of the log file
    assert(log.seekg(offsetFromEnd, std::ios_base::end));
    assert(log.read((char*) &offset, sizeof(offset)));
    assert(log.read((char*) &num, sizeof(num)));
    assert(log.read((char*) &checksum, sizeof(checksum)));

    if(verboseLevel > 5)
        cout << getLogFileName() << ": offset=" << offset << ", kv entries=" << num
                << ", checksum="  << checksum << endl;

    // seek to the beginning of the footer section and read every entry.
    assert(log.seekg(offset, std::ios_base::beg));
    KeyLen keyLen;
    std::shared_ptr<char> keyBuffer = std::make_shared<char>(MAX_KEY_LEN);
    ValueLen valLen;
    Offset kvoffset;
    SequenceNumber seqNum;
    Flag flag;
    for(int i=0; i < num; i++) {
        assert(log.read((char*) &keyLen, sizeof(keyLen)));
        assert(log.read((char*) keyBuffer.get(), keyLen));
        assert(log.read((char*) &valLen, sizeof(ValueLen)));
        assert(log.read((char*) &kvoffset, sizeof(Offset)));
        assert(log.read((char*) &seqNum, sizeof(seqNum)));
        assert(log.read((char*) &flag, sizeof(flag)));
        entries.emplace_back(keyLen, string(keyBuffer.get(), keyLen), valLen, kvoffset, seqNum, flag);
    }
}

// close an active log file and rename it
// 
// While it is possible that an active log file has been opened for read
// when we try to rename it. But luckily, it is safe to rename. 
// from rename manpage: 
//     Open file descriptors for oldpath are also unaffected.
//
void LogFile::closeRename() {
    log.flush();
    log.close();

    string activeLog = getActiveLogFileName();
    string log = getLogFileName();
    if(rename(activeLog.c_str(), log.c_str()) != 0) {
        cerr << "rename active log failed for " << activeLog 
            <<  ", error is " << strerror(errno) << endl;
        exit(-1);
    }
}

bool Logger::append(const string& key, const string& value, 
                    Flag flag, SequenceNumber seqNum, LogLocator& fo) {
    Offset offset = log->getOffset();
    if(offset + key.size()+value.size()+KVRecordMetaDataLen > MAX_LOG_FILE_SIZE) {
        log->serializeFooter();
        log->closeRename();
        log = new LogFile(dir);
    }
    log->append(key, value, flag, seqNum, fo);
    return true;
}

bool Logger::appendDelete(const std::string& key, Flag flag, SequenceNumber seqNum)
{
    string val;
    LogLocator loc;
    return append(key, val, flag, seqNum, loc);
}