#include <iostream>
#include <thread>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "transport.h"

class TCPServer {
public:
    TCPServer(int port) : port(port), server_fd(-1) {
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

        if (listen(server_fd, 3) < 0) {
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
        char buffer[1024] = {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            std::cout << "Received: " << buffer << std::endl;
            // Process the request and send a response
            send(client_fd, buffer, bytes_read, 0);
        }
        close(client_fd);
    }
};