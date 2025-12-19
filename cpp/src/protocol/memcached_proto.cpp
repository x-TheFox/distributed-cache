#include "memcached_proto.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <iostream>

class MemcachedProtocol {
public:
    MemcachedProtocol() = default;

    void processRequest(const std::string& request, std::string& response) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto command = parseCommand(request);
        if (command == "GET") {
            handleGet(request, response);
        } else if (command == "SET") {
            handleSet(request, response);
        } else {
            response = "ERROR: Unknown command";
        }
    }

private:
    std::unordered_map<std::string, std::string> cache_;
    std::mutex mutex_;

    std::string parseCommand(const std::string& request) {
        size_t spacePos = request.find(' ');
        return (spacePos == std::string::npos) ? request : request.substr(0, spacePos);
    }

    void handleGet(const std::string& request, std::string& response) {
        std::string key = request.substr(4); // Assuming "GET " is 4 characters
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            response = it->second;
        } else {
            response = "ERROR: Key not found";
        }
    }

    void handleSet(const std::string& request, std::string& response) {
        size_t spacePos1 = request.find(' ', 4);
        size_t spacePos2 = request.find(' ', spacePos1 + 1);
        if (spacePos1 == std::string::npos || spacePos2 == std::string::npos) {
            response = "ERROR: Invalid SET command";
            return;
        }
        std::string key = request.substr(4, spacePos1 - 4);
        std::string value = request.substr(spacePos2 + 1);
        cache_[key] = value;
        response = "STORED";
    }
};