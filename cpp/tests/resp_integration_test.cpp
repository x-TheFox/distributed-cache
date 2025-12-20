#include <gtest/gtest.h>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

#include "cache/cache.h"
#include "network/tcp_server.h"

static std::string send_recv(const std::string &req) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(6380);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) { close(sock); return ""; }
    send(sock, req.data(), req.size(), 0);
    char buf[4096];
    ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(sock); return ""; }
    buf[n] = '\0';
    std::string out(buf, (size_t)n);
    close(sock);
    return out;
}

TEST(RespIntegration, SetGet) {
    Cache cache(100);
    TCPServer server(6380, &cache, 2);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string setreq = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    auto r1 = send_recv(setreq);
    EXPECT_EQ(r1, "+OK\r\n");

    std::string getreq = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    auto r2 = send_recv(getreq);
    EXPECT_EQ(r2, "$3\r\nbar\r\n");

    server.stop();
}
