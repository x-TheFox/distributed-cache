#include <gtest/gtest.h>
#include "protocol/resp.h"
#include "cache/cache.h"

TEST(RespParserTest, SimpleSetParse) {
    std::string req = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
    size_t consumed = 0;
    auto parsed = RespParser::parse(req, consumed);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(consumed, req.size());
    auto v = parsed.value();
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "SET");
    EXPECT_EQ(v[1], "key");
    EXPECT_EQ(v[2], "value");
}

TEST(RespProtocolTest, SetGetDel) {
    Cache c(10);
    RespProtocol proto(&c);

    std::vector<std::string> setcmd = {"SET", "hello", "world"};
    auto resp = proto.process(setcmd);
    EXPECT_EQ(resp, "+OK\r\n");

    std::vector<std::string> getcmd = {"GET", "hello"};
    auto r2 = proto.process(getcmd);
    EXPECT_EQ(r2, "$5\r\nworld\r\n");

    std::vector<std::string> delcmd = {"DEL", "hello"};
    auto r3 = proto.process(delcmd);
    EXPECT_EQ(r3, ":1\r\n");

    auto r4 = proto.process(getcmd);
    EXPECT_EQ(r4, "$-1\r\n");
}
