#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <iostream>
#include <thread>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#include "cache/cache.h"
#include "net/reactor.h"
#include <memory>

class TCPServer {
public:
    // Accept a pointer to the canonical Cache instance
    TCPServer(int port, Cache *cache, size_t workers = 4) : port(port), cache_(cache), workers_(workers) {}

    ~TCPServer() {
        stop();
    }

    // start reactor in background thread
    void start() {
        reactor_ = std::make_unique<Reactor>(port, cache_, workers_);
        reactor_thread_ = std::thread([this]{ reactor_->run(); });
    }

    void stop() {
        if (reactor_) reactor_->stop();
        if (reactor_thread_.joinable()) reactor_thread_.join();
        reactor_.reset();
    }

private:
    int port;
    Cache *cache_;
    size_t workers_;

    std::unique_ptr<Reactor> reactor_;
    std::thread reactor_thread_;
};

#endif // TCP_SERVER_H
