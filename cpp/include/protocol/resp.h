#ifndef RESP_H
#define RESP_H

#include <string>
#include <string_view>
#include <vector>
#include <optional>

// Minimal RESP (Redis Serialization Protocol) parser and helper.
// Supports Array of Bulk Strings (e.g., *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n)

class RespParser {
public:
    // Parse a single RESP message from `data` starting at 0.
    // On success returns argument vector and sets `consumed` bytes.
    // On incomplete returns std::nullopt (caller should read more data);
    // On parse error returns empty vector (but sets consumed to 0).
    static std::optional<std::vector<std::string>> parse(const std::string_view &data, size_t &consumed);

private:
    static bool parseInteger(const std::string_view &s, size_t &pos, long long &out);
    static bool readLine(const std::string_view &s, size_t &pos, std::string &out);
};


// Simple RESP responder that executes a small command set against a `Cache`.
// Supported commands: GET key, SET key value, DEL key

class Cache; // forward

class RespProtocol {
public:
    explicit RespProtocol(Cache *cache);

    // process a parsed command vector and return RESP-encoded reply
    std::string process(const std::vector<std::string> &args);

private:
    Cache *cache_;
};

#endif // RESP_H
