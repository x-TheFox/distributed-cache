#ifdef __linux__

#include "net/event_poller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>

namespace net {

class EpollPoller : public EventPoller {
public:
    EpollPoller() {
        epfd_ = epoll_create1(0);
        if (epfd_ < 0) throw std::runtime_error("epoll_create1 failed");
    }
    ~EpollPoller() override { if (epfd_ >= 0) close(epfd_); }

    bool add_fd(int fd) override {
        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET; // edge-triggered reads
        ev.data.fd = fd;
        return epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == 0;
    }
    bool mod_fd(int fd) override {
        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        return epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == 0;
    }
    bool remove_fd(int fd) override {
        return epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
    }

    std::vector<PollEvent> wait(int timeout_ms) override {
        const int MAX_EVENTS = 64;
        struct epoll_event evs[MAX_EVENTS];
        int n = epoll_wait(epfd_, evs, MAX_EVENTS, timeout_ms);
        std::vector<PollEvent> out;
        if (n <= 0) return out;
        out.reserve(n);
        for (int i = 0; i < n; ++i) {
            int events = 0;
            if (evs[i].events & (EPOLLIN)) events |= POLLIN;
            if (evs[i].events & (EPOLLHUP)) events |= POLLHUP;
            if (evs[i].events & (EPOLLERR)) events |= POLLERR;
            out.push_back({evs[i].data.fd, events});
        }
        return out;
    }

private:
    int epfd_ = -1;
};

EventPoller* CreateDefaultPoller() { return new EpollPoller(); }

} // namespace net

#endif // __linux__
