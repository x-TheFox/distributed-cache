#ifndef MEMCACHED_PROTO_H
#define MEMCACHED_PROTO_H

#include <string>
#include <unordered_map>
#include <mutex>

class MemcachedProtocol {
public:
    MemcachedProtocol() = default;
    void processRequest(const std::string& request, std::string& response);

private:
    std::unordered_map<std::string, std::string> cache_;
    std::mutex mutex_;

    std::string parseCommand(const std::string& request);
    void handleGet(const std::string& request, std::string& response);
    void handleSet(const std::string& request, std::string& response);
};

#endif // MEMCACHED_PROTO_H
