#ifndef _LOGKV_COMMON_H
#define _LOGKV_COMMON_H

extern int verboseLevel;    // only declare here. Need to define in a source file.

#define FLAG_BIT_DELETE 0   // right-most bit for delete
#define MAX_KEY_LEN 1024    // max key length is 1024 bytes
                            // we define this maximum key length, because
                            // we need to allocate a common buffer for storing all 
                            // keys in a log file during deserialization.
#define GC_INTERVAL 5s      // GC interval in seconds.
#define GC_THRESHOLD 0.7    // if less than 70% data is valid, reclaim it.
#include <cstdint>

typedef uint32_t CheckSum;
typedef uint16_t Flag;
typedef uint64_t SequenceNumber;
typedef uint16_t KeyLen; 
typedef uint32_t ValueLen;
typedef uint64_t FileID;
typedef uint32_t Offset;

inline bool FlagDeleteBit(Flag flag) {
    return flag & (1 << FLAG_BIT_DELETE);
}
#endif
