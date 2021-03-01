/*
 * LogKV Client 
 */ 

#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include "./gen-cpp/LogKeyValue.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

int main() {
    std::shared_ptr<TTransport> socket(new TSocket("127.0.0.1", 9090));
    std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    LogKeyValueClient client(protocol);
    Response ret;
    try {
        string command, key("key1"), value("myvalue");
        transport->open();
        //for(int i=0; i < 100000; i++)
        //    client.Put(key, value);

       
        cout << "Enter a command: " << endl;
        while(cin >> command >> key) {
            if(command == "put") {
                cin >> value;
                client.Put(ret, key, value);
                if(!ret.status)
                    cout << "put failed" << endl;

            } else if (command == "get") {
                
                client.Get(ret, key);
                if(ret.status)
                    cout << "value is: " << ret.value << endl;
                else 
                    cout << "no such key" << endl;
            } else if (command == "del") {
                client.Delete(ret, key);
                if(!ret.status)
                    cout << "delete failed" << endl;
            } else if (command == "exist") {
                client.Exist(ret, key);
                if(ret.status) {
                    cout << "key " << key << " exists." << endl;
                } else {
                    cout << "key " << key << " does not exist." << endl;
                }
            } else 
                cout << "command not support" << endl;

            cout << "Enter a command: " << endl;
        }

        transport->close();
    } catch (TException& tx) {
        cerr << "Error: " << tx.what() << endl;
    }
}
