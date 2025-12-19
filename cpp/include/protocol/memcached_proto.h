#ifndef MEMCACHED_PROTO_H
#define MEMCACHED_PROTO_H

#include <string>

class MemcachedProtocol {
public:
    MemcachedProtocol() = default;
    void processRequest(const std::string& request, std::string& response);
};

#endif // MEMCACHED_PROTO_H
