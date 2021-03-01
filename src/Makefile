CC = g++ -std=c++17
MAKE = make

test: test.o Log.o
	$(CC) -o test  test.o Log.o -lstdc++fs

test.o: test.cpp
	$(CC) -c test.cpp

Log.o: Log.cpp
	$(CC) -g -Og -c Log.cpp

client: Client.cpp
	$(MAKE) -C gen-cpp all
	$(CC) -o client Client.cpp gen-cpp/LogKeyValue.o gen-cpp/RPC_types.o -lthrift

server: Log.o Server.cpp
	$(MAKE) -C gen-cpp all
	$(CC) -Og -g -o server Server.cpp gen-cpp/LogKeyValue.o gen-cpp/RPC_types.o Log.o -pthread -lthrift -lstdc++fs

clean:
	$(MAKE) -C gen-cpp clean
	rm -f test client server *.o
