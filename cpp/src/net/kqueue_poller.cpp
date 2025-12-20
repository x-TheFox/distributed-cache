#ifdef __APPLE__

#include "net/event_poller.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#include <stdexcept>

namespace net {

class KqueuePoller : public EventPoller {
public:
    KqueuePoller() {
        kq_ = kqueue();
        if (kq_ < 0) throw std::runtime_error("kqueue() failed");
    }
    ~KqueuePoller() override { if (kq_ >= 0) close(kq_); }

    bool add_fd(int fd) override {
        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, nullptr);
        return kevent(kq_, &ev, 1, nullptr, 0, nullptr) == 0;
    }
    bool mod_fd(int fd) override { return true; }
    bool remove_fd(int fd) override {
        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        return kevent(kq_, &ev, 1, nullptr, 0, nullptr) == 0;
    }

    std::vector<PollEvent> wait(int timeout_ms) override {
        const int MAX_EVENTS = 64;
        struct kevent evs[MAX_EVENTS];
        struct timespec ts;
        struct timespec *tsp = nullptr;
        if (timeout_ms >= 0) {
            ts.tv_sec = timeout_ms / 1000;
            ts.tv_nsec = (timeout_ms % 1000) * 1000000;
            tsp = &ts;
        }
        int n = kevent(kq_, nullptr, 0, evs, MAX_EVENTS, tsp);
        std::vector<PollEvent> out;
        if (n <= 0) return out;
        out.reserve(n);
        for (int i = 0; i < n; ++i) {
            int events = 0;
            if (evs[i].filter == EVFILT_READ) events |= POLLIN;
            if (evs[i].flags & EV_EOF) events |= POLLHUP;
            out.push_back({(int)evs[i].ident, events});
        }
        return out;
    }

private:
    int kq_ = -1;
};

EventPoller* CreateDefaultPoller() { return new KqueuePoller(); }

} // namespace net

#endif // __APPLE__
