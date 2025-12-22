#include <gtest/gtest.h>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

#include "cache/cache.h"
#include "network/tcp_server.h"

static std::string send_recv_port(int port, const std::string &req) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) { close(sock); return ""; }
    send(sock, req.data(), req.size(), 0);
    char buf[8192];
    ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(sock); return ""; }
    buf[n] = '\0';
    std::string out(buf, (size_t)n);
    close(sock);
    return out;
}

TEST(RespPipelining, MultipleCommandsSingleWrite) {
    Cache cache(100);
    TCPServer server(6382, &cache, 2);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // SET followed immediately by GET in single write (pipelined)
    std::string req = "*3\r\n$3\r\nSET\r\n$2\r\nk1\r\n$1\r\nv\r\n*2\r\n$3\r\nGET\r\n$2\r\nk1\r\n";
    auto r = send_recv_port(6382, req);

    EXPECT_NE(r.find("+OK\r\n"), std::string::npos);
    EXPECT_NE(r.find("$1\r\nv\r\n"), std::string::npos);

    server.stop();
}
