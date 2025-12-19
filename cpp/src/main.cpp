#include <iostream>
#include "cache/cache.h"
#include "network/transport.h"

int main(int argc, char* argv[]) {
    // Initialize the cache system
    Cache cache;

    // Start the server (TCP or UDP based on command line argument)
    if (argc > 1 && std::string(argv[1]) == "tcp") {
        TcpServer tcpServer(&cache);
        tcpServer.start();
    } else {
        UdpServer udpServer(&cache);
        udpServer.start();
    }

    return 0;
}