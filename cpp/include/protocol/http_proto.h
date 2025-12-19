#ifndef HTTP_PROTO_H
#define HTTP_PROTO_H

#include <string>

class HttpProtocol {
public:
    HttpProtocol() = default;
    std::string handleRequest(const std::string& request);
};

#endif // HTTP_PROTO_H
