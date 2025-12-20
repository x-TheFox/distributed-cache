#ifndef REACTOR_H
#define REACTOR_H

#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>
#include <atomic>
#include <memory>
#include <sys/types.h>
#include "net/event_poller.h"

class ThreadPool;
class Cache;

// Reactor: uses poll() to wait for events on server socket and client sockets.
// On readable data, accumulates into per-client buffer and submits tasks to thread pool to parse/process requests.
class Reactor {
public:
    Reactor(int port, Cache *cache, size_t worker_threads = 4);
    ~Reactor();

    // start the reactor loop (blocking) -- caller may run it on a dedicated thread
    void run();

    // shutdown reactor
    void stop();

private:
    int port_;
    int server_fd_ = -1;
    Cache *cache_;

    std::unique_ptr<ThreadPool> pool_;
    std::unique_ptr<net::EventPoller> poller_;
    std::atomic<bool> running_{false};

    // per-client buffers (fd -> buffer)
    std::mutex clients_mutex_;
    std::unordered_map<int, std::string> buffers_;

    void setupServer();
    void acceptClient();
    void handleClientRead(int client_fd);
    void closeClient(int client_fd);
};

#endif // REACTOR_H
