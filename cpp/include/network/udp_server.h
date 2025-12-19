#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>

class UDPServer {
public:
    UDPServer(int port) : port(port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error("Failed to bind socket");
        }
    }

    ~UDPServer() {
        close(sockfd);
    }

    void start() {
        std::cout << "UDP Server listening on port " << port << std::endl;
        while (true) {
            char buffer[1024];
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &len);
            if (n < 0) {
                std::cerr << "Error receiving data" << std::endl;
                continue;
            }
            buffer[n] = '\0';
            std::cout << "Received: " << buffer << std::endl;
            sendto(sockfd, buffer, n, 0, (const struct sockaddr *)&client_addr, len);
        }
    }

private:
    int sockfd;
    int port;
    struct sockaddr_in server_addr;
};

#endif // UDP_SERVER_H
