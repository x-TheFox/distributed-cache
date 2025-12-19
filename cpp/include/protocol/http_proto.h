#ifndef HTTP_PROTO_H
#define HTTP_PROTO_H

#include <string>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <mutex>

class HttpProtocol {
public:
    HttpProtocol() = default;
    std::string handleRequest(const std::string& request);

private:
    std::unordered_map<std::string, std::string> cache;
    std::mutex cacheMutex;

    std::string handleGet(const std::string& path);
    std::string handlePost(std::istringstream& requestStream);
};

#endif // HTTP_PROTO_H
