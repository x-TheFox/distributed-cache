#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "cache/cache.h"
#include "network/tcp_server.h"
#include "protocol/resp.h"
#include "metrics/metrics.h"

// Simple SyncServer (thread-per-connection) for comparison
class SyncServer {
public:
    SyncServer(int port, Cache *cache) : port_(port), cache_(cache) {}
    void start() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        int reuse = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);
        bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr));
        listen(server_fd_, 128);
        accepting_ = true;
        accept_thread_ = std::thread([this]{ this->acceptLoop(); });
    }
    void stop() {
        accepting_ = false;
        if (server_fd_ != -1) close(server_fd_);
        if (accept_thread_.joinable()) accept_thread_.join();
        for (auto &t : threads_) if (t.joinable()) t.join();
    }
private:
    void acceptLoop() {
        while (accepting_) {
            int c = accept(server_fd_, nullptr, nullptr);
            if (c < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            threads_.emplace_back([this, c]{ this->handleClient(c); });
        }
    }
    void handleClient(int fd) {
        char buf[4096];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) { close(fd); return; }
        std::string s(buf, (size_t)n);
        size_t consumed = 0;
        auto parsed = RespParser::parse(s, consumed);
        RespProtocol proto(cache_);
        auto start = std::chrono::steady_clock::now();
        std::string reply = proto.process(parsed.value());
        auto end = std::chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        Metrics::instance().record_latency_us((uint64_t)us);
        send(fd, reply.data(), reply.size(), 0);
        close(fd);
    }

    int port_;
    int server_fd_ = -1;
    Cache *cache_;
    std::atomic<bool> accepting_{false};
    std::thread accept_thread_;
    std::vector<std::thread> threads_;
};


static void client_worker(int port, int ops, int id) {
    for (int i = 0; i < ops; ++i) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv{};
        serv.sin_family = AF_INET;
        serv.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
        connect(sock, (struct sockaddr*)&serv, sizeof(serv));
        std::string k = "k" + std::to_string((i%100));
        std::string v = "v" + std::to_string(i);
        std::string req = "*3\r\n$3\r\nSET\r\n$" + std::to_string(k.size()) + "\r\n" + k + "\r\n$" + std::to_string(v.size()) + "\r\n" + v + "\r\n";
        auto s = std::chrono::steady_clock::now();
        send(sock, req.data(), req.size(), 0);
        char buf[1024];
        ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
        auto e = std::chrono::steady_clock::now();
        if (n <= 0) { close(sock); continue; }
        uint64_t us = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(e - s).count();
        Metrics::instance().record_latency_us(us);
        close(sock);
    }
}

int main(int argc, char** argv) {
    int mode_async = 1; // 1=async, 0=sync
    if (argc > 1) mode_async = atoi(argv[1]);

    Cache cache(10000);
    int port = 6390;
    SyncServer syncSrv(port, &cache);
    TCPServer asyncSrv(port, &cache, 8);

    if (mode_async) {
        asyncSrv.start();
    } else {
        syncSrv.start();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int clients = 50;
    int ops = 200; // per client
    if (argc > 2) clients = atoi(argv[2]);
    if (argc > 3) ops = atoi(argv[3]);
    std::vector<std::thread> clients_threads;
    for (int i = 0; i < clients; ++i) clients_threads.emplace_back(client_worker, port, ops, i);
    for (auto &t : clients_threads) t.join();

    // allow pending server tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    double p99 = Metrics::instance().percentile(99.0);
    double p999 = Metrics::instance().percentile(99.9);

    std::cout << (mode_async ? "async" : "sync") << " clients=" << clients << " ops="<< ops << " p99_us="<< p99 << " p99.9_us="<< p999 <<"\n";

    if (mode_async) asyncSrv.stop(); else syncSrv.stop();
    return 0;
}
