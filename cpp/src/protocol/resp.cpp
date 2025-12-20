#include "protocol/resp.h"
#include "cache/cache.h"
#include <sstream>
#include <cstdlib>

using namespace std;

bool RespParser::readLine(const std::string_view &s, size_t &pos, std::string &out) {
    size_t nl = s.find("\r\n", pos);
    if (nl == std::string_view::npos) return false;
    out.assign(s.data() + pos, nl - pos);
    pos = nl + 2;
    return true;
}

bool RespParser::parseInteger(const std::string_view &s, size_t &pos, long long &out) {
    std::string ln;
    if (!readLine(s, pos, ln)) return false;
    try {
        out = stoll(ln);
    } catch (...) {
        return false;
    }
    return true;
}

optional<vector<string>> RespParser::parse(const std::string_view &data, size_t &consumed) {
    consumed = 0;
    if (data.empty()) return nullopt;
    size_t pos = 0;
    if (data[pos] != '*') return vector<string>{}; // protocol error -> empty vector
    pos++;
    long long elements;
    if (!parseInteger(data, pos, elements)) return nullopt; // parse number of elements
    if (elements < 1) return vector<string>{};

    vector<string> out;
    out.reserve(elements);

    for (long long i = 0; i < elements; ++i) {
        if (pos >= data.size()) return nullopt;
        if (data[pos] != '$') return vector<string>{}; // protocol error
        pos++;
        long long len;
        if (!parseInteger(data, pos, len)) return nullopt;
        if (len < 0) {
            // nil bulk string
            out.emplace_back();
            continue;
        }
        if (pos + (size_t)len + 2 > data.size()) return nullopt; // need more
        out.emplace_back(data.data() + pos, (size_t)len);
        pos += (size_t)len;
        // must end with CRLF
        if (data.size() < pos + 2) return nullopt;
        if (data[pos] != '\r' || data[pos+1] != '\n') return vector<string>{};
        pos += 2;
    }

    consumed = pos;
    return out;
}

// RESP helpers
static string simpleString(const string &s) { return "+" + s + "\r\n"; }
static string bulkString(const string &s) { return "$" + to_string(s.size()) + "\r\n" + s + "\r\n"; }
static string nullBulk() { return "$-1\r\n"; }
static string integerReply(long long i) { return ":" + to_string(i) + "\r\n"; }
static string errReply(const string &e) { return "-" + e + "\r\n"; }

RespProtocol::RespProtocol(Cache *cache) : cache_(cache) {}

string RespProtocol::process(const vector<string> &args) {
    if (args.empty()) return errReply("ERR empty command");
    string cmd = args[0];
    // make uppercase
    for (auto &c : cmd) c = toupper((unsigned char)c);

    if (cmd == "PING") {
        return simpleString("PONG");
    } else if (cmd == "GET") {
        if (args.size() != 2) return errReply("ERR wrong number of arguments for 'get'");
        auto v = cache_->get(args[1]);
        if (!v.has_value()) return nullBulk();
        return bulkString(v.value());
    } else if (cmd == "SET") {
        if (args.size() != 3) return errReply("ERR wrong number of arguments for 'set'");
        cache_->put(args[1], args[2]);
        return simpleString("OK");
    } else if (cmd == "DEL") {
        if (args.size() != 2) return errReply("ERR wrong number of arguments for 'del'");
        bool removed = cache_->remove(args[1]);
        return integerReply(removed ? 1 : 0);
    }

    return errReply("ERR unknown command");
}
