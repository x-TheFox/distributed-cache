#include "net/thread_pool.h"
#include <stdexcept>

ThreadPool::ThreadPool(size_t n) {
    if (n == 0) n = 1;
    for (size_t i = 0; i < n; ++i) {
        threads_.emplace_back([this]{
            while (running_) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->q_mutex_);
                    this->q_cv_.wait(lock, [this]{ return !this->q_.empty() || !this->running_; });
                    if (!this->running_ && this->q_.empty()) return;
                    task = std::move(this->q_.front());
                    this->q_.pop();
                }
                try {
                    task();
                } catch (...) {}
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::enqueue(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lock(q_mutex_);
        q_.push(std::move(fn));
    }
    q_cv_.notify_one();
}

void ThreadPool::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    q_cv_.notify_all();
    for (auto &t : threads_) if (t.joinable()) t.join();
}
