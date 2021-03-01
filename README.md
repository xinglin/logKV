# LogKV
A key-value store with a log-structured durable storage.

The key-value server (src/Server.cpp) accepts Put/Get/Delete/Exist RPC requests,
using Apache Thrift. An example client program (src/Client.cpp) is also implemented.


An in-memory hash map maps a key to its location stored in a log file.
It always points a key to its latest version. If a key does not exist
in the hash map, it is either deleted or there is no Put for this key.

At any time, logKV opens three active log files to append new records:
PutLog, DelLog and GCLog. New Puts are appended to the active PutLog.
Delete requests are appended to DelLog and Puts from garbage collection
are appended to GCLog. The separate GCLog is used to hopefully separate
existing/cold KV pairs from new/hot KV pairs. Once an active log file
reaches a certain size, it is closed and a new active log file is opened.
The log file size limit is defined as MAX_LOG_FILE_SIZE in src/Log.cpp. 
We use a default size of 100 MiB.

Garbage collection is implemented as a background thread.
It is scheduled to run every 5 seconds. 
During the first pass, it scans log files with KV records (PutLogs and GCLogs) based on their creation time and reclaim a log file if its utilization is 
below 70%. During the second pass, DelLogs are scanned and reclaimed if 
their utilization becomes lower than 70%.

# How to Use?
<pre>
$ cd src; make server; make client

Create a dir for logkv to work with
$ mkdir logkv

Start the kv server
$ ./server -d logkv

At another terminal:
$ ./client
xing@debian10-dev ~/d/l/src> ./client
Enter a command: 
put k1 v1<Enter>
Enter a command: 
put k2 v2<Enter>
Enter a command: 
get k1<Enter>
value is: v1
Enter a command: 
</pre>

Please refer to doc/design.md for more details. Enjoy!
