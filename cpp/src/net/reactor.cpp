#include "net/reactor.h"
#include "net/thread_pool.h"
#include "cache/cache.h"
#include "protocol/resp.h"
#include "metrics/metrics.h"

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>

static int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

Reactor::Reactor(int port, Cache *cache, size_t worker_threads) : port_(port), cache_(cache) {
    pool_ = std::make_unique<ThreadPool>(worker_threads);
    setupServer();
}

Reactor::~Reactor() {
    stop();
    if (server_fd_ != -1) close(server_fd_);
}

void Reactor::setupServer() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) { perror("socket"); exit(1); }
    int reuse = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(server_fd_, 128) < 0) { perror("listen"); exit(1); }
    if (setNonBlocking(server_fd_) < 0) { perror("fcntl"); }
}

void Reactor::run() {
    running_ = true;
    std::vector<struct pollfd> fds;
    fds.reserve(256);
    while (running_) {
        fds.clear();
        struct pollfd srv{};
        srv.fd = server_fd_;
        srv.events = POLLIN;
        fds.push_back(srv);

        // build client fds
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (auto &p : buffers_) {
                struct pollfd c{};
                c.fd = p.first;
                c.events = POLLIN;
                fds.push_back(c);
            }
        }

        int timeout_ms = 100; // check periodically
        int rc = poll(fds.data(), fds.size(), timeout_ms);
        if (rc < 0) { if (errno == EINTR) continue; perror("poll"); break; }
        if (rc == 0) continue;

        for (size_t i = 0; i < fds.size(); ++i) {
            auto &pf = fds[i];
            if (pf.revents & POLLIN) {
                if (pf.fd == server_fd_) {
                    acceptClient();
                } else {
                    // schedule read processing
                    int fd = pf.fd;
                    pool_->enqueue([this, fd]{ this->handleClientRead(fd); });
                }
            }
            if (pf.revents & (POLLHUP | POLLERR | POLLNVAL)) {
                if (pf.fd != server_fd_) closeClient(pf.fd);
            }
        }
    }
}

void Reactor::stop() {
    running_ = false;
    if (pool_) pool_->stop();
}

void Reactor::acceptClient() {
    while (true) {
        int cfd = accept(server_fd_, nullptr, nullptr);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            perror("accept"); return;
        }
        if (setNonBlocking(cfd) < 0) perror("setnonblock");
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            buffers_[cfd] = std::string();
        }
    }
}

void Reactor::closeClient(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = buffers_.find(client_fd);
    if (it != buffers_.end()) buffers_.erase(it);
    close(client_fd);
}

void Reactor::handleClientRead(int client_fd) {
    char tmp[4096];
    ssize_t n = read(client_fd, tmp, sizeof(tmp));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data right now; do not close
            return;
        }
        closeClient(client_fd); return;
    }
    if (n == 0) { closeClient(client_fd); return; }
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        buffers_[client_fd].append(tmp, (size_t)n);
    }

    // try to parse one message
    std::string buffer;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        buffer = buffers_[client_fd];
    }

    size_t consumed = 0;
    auto parsed = RespParser::parse(std::string_view(buffer), consumed);
    if (!parsed.has_value()) {
        // need more data or invalid: if invalid respond with error and close
        // We detect malformed by returning empty vector
        // if parse returned nullopt -> incomplete
        if (consumed == 0) {
            // incomplete, wait for more
            return;
        } else {
            std::string err = "-ERR malformed request\r\n";
            send(client_fd, err.data(), err.size(), 0);
            closeClient(client_fd);
            return;
        }
    }

    // measure start time
    auto start = std::chrono::steady_clock::now();
    RespProtocol proto(cache_);
    std::string reply = proto.process(parsed.value());
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    Metrics::instance().record_latency_us((uint64_t)us);

    // send reply
    send(client_fd, reply.data(), reply.size(), 0);

    // remove consumed bytes from buffer
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (consumed <= buffers_[client_fd].size()) {
            buffers_[client_fd].erase(0, consumed);
        } else buffers_[client_fd].clear();
    }
}
