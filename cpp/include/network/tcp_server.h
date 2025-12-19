#ifndef TCP_SERVER_H
#define TCP_SERVER_H

class TCPServer {
public:
    explicit TCPServer(int port);
    void start();
};

#endif // TCP_SERVER_H
