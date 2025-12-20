#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <iostream>
#include <thread>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#include "cache/cache.h"
#include "protocol/resp.h"

class TCPServer {
public:
    // Accept a pointer to the canonical Cache instance
    TCPServer(int port, Cache *cache) : port(port), server_fd(-1), cache_(cache) {
        setupServer();
    }

    ~TCPServer() {
        if (server_fd != -1) {
            close(server_fd);
        }
    }

    void start() {
        listenForConnections();
    }

private:
    int port;
    int server_fd;
    Cache *cache_;

    void setupServer() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("Bind failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 128) < 0) {
            perror("Listen failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
    }

    void listenForConnections() {
        while (true) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0) {
                perror("Accept failed");
                continue;
            }
            std::thread(&TCPServer::handleClient, this, client_fd).detach();
        }
    }

    void handleClient(int client_fd) {
        std::string buf;
        buf.reserve(4096);
        char tmp[1024];
        ssize_t n;
        // Read once for simplicity; the RESP parser supports incomplete messages
        n = read(client_fd, tmp, sizeof(tmp));
        if (n <= 0) { close(client_fd); return; }
        buf.append(tmp, (size_t)n);

        size_t consumed = 0;
        auto parsed = RespParser::parse(buf, consumed);
        if (!parsed.has_value()) {
            // incomplete or malformed
            std::string resp = "-ERR incomplete or invalid request\r\n";
            send(client_fd, resp.data(), resp.size(), 0);
            close(client_fd);
            return;
        }

        RespProtocol proto(cache_);
        std::string reply = proto.process(parsed.value());
        send(client_fd, reply.data(), reply.size(), 0);
        close(client_fd);
    }
};

#endif // TCP_SERVER_H
