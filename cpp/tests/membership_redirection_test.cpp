#include <gtest/gtest.h>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

#include "sharder/membership_service.h"
#include "cache/cache.h"
#include "network/tcp_server.h"
#include "sharder/router.h"

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

TEST(MembershipRedirection, MovedForRemoteKey) {
    // Initialize membership service with local node info
    LocalNodeInfo local{"nodeA", "127.0.0.1", 6384};
    MembershipService ms(local, 10);

    // Add two nodes (this triggers rebuildRouterAndSwap via OnNodeJoin)
    ms.OnNodeJoin("nodeA", "127.0.0.1", 6384);
    ms.OnNodeJoin("nodeB", "127.0.0.1", 6385);

    // Ensure the global router is set
    auto router = Router::get_default();
    ASSERT_NE(router, nullptr);

    // start server bound to local nodeA's port
    Cache cache(100);
    TCPServer server(6384, &cache, 2);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // find a key that maps to nodeB
    std::string remote_key;
    Router::Route remote_route;
    for (int i = 0; i < 10000; ++i) {
        std::string k = "key" + std::to_string(i);
        auto r = router->lookup(k);
        if (r.type == Router::RouteType::REMOTE) {
            if (r.ip == "127.0.0.1" && r.port == 6385) {
                remote_key = k; remote_route = r; break;
            }
        }
    }
    ASSERT_FALSE(remote_key.empty());

    std::string setreq = "*3\r\n$3\r\nSET\r\n$" + std::to_string(remote_key.size()) + "\r\n" + remote_key + "\r\n$1\r\nv\r\n";
    auto r = send_recv_port(6384, setreq);
    std::string expected = "-MOVED " + std::to_string(remote_route.slot) + " " + remote_route.ip + ":" + std::to_string(remote_route.port) + "\r\n";
    EXPECT_EQ(r, expected);

    // pick a key that maps to nodeA (local)
    std::string local_key;
    for (int i = 0; i < 10000; ++i) {
        std::string k = "lkey" + std::to_string(i);
        auto r2 = router->lookup(k);
        if (r2.type == Router::RouteType::LOCAL) { local_key = k; break; }
    }
    ASSERT_FALSE(local_key.empty());
    std::string setreq2 = "*3\r\n$3\r\nSET\r\n$" + std::to_string(local_key.size()) + "\r\n" + local_key + "\r\n$1\r\nx\r\n";
    auto r2 = send_recv_port(6384, setreq2);
    EXPECT_EQ(r2, "+OK\r\n");

    server.stop();
}
