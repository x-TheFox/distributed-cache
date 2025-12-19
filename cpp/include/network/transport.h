#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

class Transport {
public:
    virtual ~Transport() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void send(const std::string& message) = 0;
    virtual std::string receive() = 0;
};

class TcpTransport : public Transport {
public:
    void start() override {
        // Implementation for starting TCP transport
        std::cout << "TCP Transport started." << std::endl;
    }

    void stop() override {
        // Implementation for stopping TCP transport
        std::cout << "TCP Transport stopped." << std::endl;
    }

    void send(const std::string& message) override {
        // Implementation for sending a message over TCP
        std::cout << "Sending over TCP: " << message << std::endl;
    }

    std::string receive() override {
        // Implementation for receiving a message over TCP
        std::string message = "Received TCP message";
        std::cout << message << std::endl;
        return message;
    }
};

class UdpTransport : public Transport {
public:
    void start() override {
        // Implementation for starting UDP transport
        std::cout << "UDP Transport started." << std::endl;
    }

    void stop() override {
        // Implementation for stopping UDP transport
        std::cout << "UDP Transport stopped." << std::endl;
    }

    void send(const std::string& message) override {
        // Implementation for sending a message over UDP
        std::cout << "Sending over UDP: " << message << std::endl;
    }

    std::string receive() override {
        // Implementation for receiving a message over UDP
        std::string message = "Received UDP message";
        std::cout << message << std::endl;
        return message;
    }
};

#endif // TRANSPORT_H