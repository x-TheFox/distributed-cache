#include "http_proto.h"
#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <mutex>

class HttpProtocol {
public:
    HttpProtocol() = default;

    std::string handleRequest(const std::string& request) {
        std::istringstream requestStream(request);
        std::string method, path;
        requestStream >> method >> path;

        if (method == "GET") {
            return handleGet(path);
        } else if (method == "POST") {
            return handlePost(requestStream);
        } else {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }
    }

private:
    std::unordered_map<std::string, std::string> cache;
    std::mutex cacheMutex;

    std::string handleGet(const std::string& path) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(path);
        if (it != cache.end()) {
            return "HTTP/1.1 200 OK\r\n\r\n" + it->second;
        } else {
            return "HTTP/1.1 404 Not Found\r\n\r\n";
        }
    }

    std::string handlePost(std::istringstream& requestStream) {
        std::string line;
        std::string body;
        while (std::getline(requestStream, line) && line != "\r") {
            // Read headers, ignore for now
        }
        std::getline(requestStream, body);
        
        std::string path = "/some_path"; // Extract path from headers or body as needed
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            cache[path] = body; // Store the body in the cache
        }
        return "HTTP/1.1 201 Created\r\n\r\n";
    }
};