#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t n);
    ~ThreadPool();

    // enqueue a work item
    void enqueue(std::function<void()> fn);

    // stop and join
    void stop();

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> q_;
    std::mutex q_mutex_;
    std::condition_variable q_cv_;
    std::atomic<bool> running_{true};
};

#endif // THREAD_POOL_H
