#include <iostream>
#include "cache/cache.h"
#include "network/transport.h"

int main(int argc, char* argv[]) {
    // Initialize the cache system with a default capacity
    Cache cache(1024);

    // Start the server (TCP or UDP based on command line argument)
    if (argc > 1 && std::string(argv[1]) == "tcp") {
        TCPServer tcpServer(8080);
        tcpServer.start();
    } else {
        UDPServer udpServer(8080);
        udpServer.start();
    }

    return 0;
}