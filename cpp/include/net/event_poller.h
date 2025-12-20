#ifndef EVENT_POLLER_H
#define EVENT_POLLER_H

#include <vector>

namespace net {

struct PollEvent {
    int fd;
    int events; // bitmask (POLLIN, POLLHUP, POLLERR-like semantics)
};

class EventPoller {
public:
    virtual ~EventPoller() = default;
    virtual bool add_fd(int fd) = 0;
    virtual bool mod_fd(int fd) = 0;
    virtual bool remove_fd(int fd) = 0;
    // wait returns events or empty if timeout/no events
    virtual std::vector<PollEvent> wait(int timeout_ms) = 0;
};

} // namespace net

#endif // EVENT_POLLER_H
