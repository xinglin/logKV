# LogKV
A log-structure key-value store

# System Architecture
The system is composed of two main components, an in-memory index and a log-structure durable storage system. For durable storage, we store data into fixed-size log files in a directory in a filesystem. The in-memory index maps a key to the offset in a log file. We assume we can store all the index in memory.
Garbage collection is running periodically to compact sparse log files and reclaim the space (we rely on the underlying filesystem to do the actual disk space management.).

# In-memory Index
It maps a key to file:offset for a kv pair.
```
struct LogLocator {
   uint64_t fileno;
   uint32_t offset;     // max is 4GB
};
```
# On-disk Format for a key/value pair
<pre>
|---------------------------------------------------------------------|  
| checksum | sequencenum | flag    | key_len | val_len | key | value  |  
| 32 bits  | 64 bits     | 16 bits | 16 bits | 32 bits | key | value  |  
|---------------------------------------------------------------------|  
</pre>

The flag field is used to mark special status of a key/value pair, for example, when a key/value pair is deleted. The maximum key length is 64 KiB and the maximum value length is 4 GiB.  

# Directory Structure
<pre>
root_dir  
  | -> log_1.kv, a full and closed log file. Only serve reads.   
  | -> log_2.kv,   
  | -> log_3.kv, 
  | -> log_4_active.kv, active log file for new appends.
  | -> log_5_active.kv,
  | -> log_6_acitve.kv,  
</pre>
# Log File Format
Use log_N.active for log files that are still receiving writes. Once a log file is full, rename it to log_N.kv.

<pre>
|------------------------------------|  
| header | key/value pairs  | footer |   
| 1 KiB  | key/value pairs  | footer |  
|------------------------------------|   

NOTE: the current implementation does not have a header section. 

footer section
|------------------------------------------------------|
|keylen | key1 | valuelen | offset | sequencenum| flag |
|keylen | key2 | valuelen | offset | sequencenum| flag |
| ...                                                  |
|   footerOffset   |    num of keys    |   checksum    |
|------------------------------------------------------|

* keylen and valueLen are needed to calculate the amount of valid/invalid kv pairs in a log file.
* footerOffset: the start offset of the footer section in the log file.
* num of keys: number of keys in this log file. 
* checksum: checksum of the footer section
</pre>

In the footer section, we store all the keys and the offset, sequencenum, and flag of each key/value pair in this log file. With this information, we can read the keys in a log file from its footer section, lookup the in-memory index and determine how sparse a log file is without reading the whole log file. 
The footer section also contributes to a quick cold startup.

# Workflow
## Cold Startup  
The main task of the cold startup is to re-build the in-memory index. 

For cold startup, we need to maintain another in-memory mapping (seqFlagMap), key -> sequencenum:flag, to identify the latest version and whether a k/v has been delete. Once the cold startup is completed, seqFlagMap is discarded.

  1. Scan the footer section of log files (log files can be scanned in any order, as we now rely on the sequence number to detect the latest version). 
  2. Compare the sequencenum of every new record with the sequence number for that kv pair in the seqFlagMap. If the new record is newer than seqFlagMap and it is not a delete record, insert new k/v pairs into the in-memory index and update the seqFlagMap. Remove a k/v pair from kvMap if the most recent record is a delete record.
 
## Get(string key) 

  1. Lookup the in-memory index
  2. If found, read the kv pair from the log, and return to the client. 
  3. If not found, return KEY_NOT_EXIST

We rely/re-use the operating system page cache here. No need to design a separate cache to store hot kv pairs in memory. 

## Put(string key, string value)

There are three types of put requests:
* regular user put requests
* regular user delete requests (isDelete flag set to true)
* put requests generated from the garbage collection process (fileOffsetA set to point to some log file)

For each type of PUT, we append them into a different log file. Three log files are opened and active for appends when the system is running.  

```
// 1. Regular Put
  Append KV to PutLog
  update kvMap

// 2. Delete
  if(not find in kvMap)
    return KEY_NOT_EXIST
  Append a delete record to DelLog
  delete key from kvMap

// 3. GC Put

  if(key is found in kvMap and location matches kvMap[key])
    Append KV to GCLog
    update kvMap
  else
    skip this KV record.
```

</pre>

      
## Delete(string key)
  convert to a put request with delete flag set to true.

## Garbage Collection

Garbage collection for a KV record (which contains both the key and its value) is straightforward: if the location of a KV pair matches that in the kvMap, this kv pair is valid. Otherwise, the kv record from the log file is out-of-date and can be reclaimed. However, delete records are tricky: say if key k1 is created at seqNum=1, stored in log_1.kv and it is deleted at seqNum=20. If log_1.kv is not reclaimed, because its utilization is still high, then the delete record for k1 should not be removed from the system. Thus, we adopted a two-pass approach for our garbage collection.

During the first pass, scan log files with KV records (PutLogs and GCLogs), according to their creation time (according to fileID actually). Valid kv pairs are appended to the active GCLog. During this process, we also track deleted keys (deletedKVMap) and the last Put for each deleted key. A delete key is a kv record that is stored in a log file but it does not exist in the kvMap. Any delete record for that key with a smaller seqNum than the last Put can be reclaimed and removed from the system. Note if a key is created and then is deleted, the Put record for this kv will be garbage collected during the next round garbage collection but the delete record will be garbage collected during the following garbage collection after the next round.

Criteria to consider log files to reclaim space:  
  1. Pick a log file which contains the largest amount of invalid (deleted) bytes 
  2. Prefer to pick an older log file over a more recent log file

Policy implemented:
  1. scan log files from oldest to newest.
  2. If 30% of a log file's data contains invalid key/value pairs, reclaim it.

<pre>
For each k/v pair in the selected log file:
  -> put(k, value, isDelete=false, fileOffset);
</pre>

NOTE: 
1. Between two log files where logA has X% invalid space and logB has Y% invalid space and X>Y, it is easy to pick logA for compaction. 
2. Between two log files with the same % of invalid space, how to pick? 
   Prefer old files for compaction because k/v pairs in old files are more stable and thus if we compact these kv pairs into new log files, fewer deletion will happen to this new log file. On the other hand, if we compact more recent log files for compaction, they may be deleted again in the new future with a higher probability: the log files may become sparse again very soon. In short, the invalid space in the older log files should be given a higher preference to be reclaimed than the more recent log files.
   
# Reference
* libtbb-dev: concurrent_hash_map/concurrent_lru_cache from Intel.
