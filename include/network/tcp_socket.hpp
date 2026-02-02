#pragma once

#include "network/socket.hpp"
#include <sys/socket.h>

namespace wowee {
namespace network {

class TCPSocket : public Socket {
public:
    TCPSocket();
    ~TCPSocket() override;

    bool connect(const std::string& host, uint16_t port) override;
    void disconnect() override;
    bool isConnected() const override { return connected; }

    void send(const Packet& packet) override;
    void update() override;

private:
    void tryParsePackets();
    size_t getExpectedPacketSize(uint8_t opcode);

    int sockfd = -1;
    bool connected = false;
    std::vector<uint8_t> receiveBuffer;
};

} // namespace network
} // namespace wowee
