#ifndef UV_NET_UDP_CONNECTION_H
#define UV_NET_UDP_CONNECTION_H

#include "connection.h"

namespace uv_net {

class UdpServer;

// UDP 伪连接实现
class UdpConnection : public Connection {
public:
    UdpServer* server_;
    uv_udp_t* socket_; // 绑定的 socket，用于发送回复
    struct sockaddr_storage addr_;
    std::string ip_;
    int port_;
    uint32_t conn_id_; // 连接ID

    UdpConnection(UdpServer* server, uv_udp_t* socket, const struct sockaddr* addr);
    ~UdpConnection() override = default;

    void Send(const char* data, size_t len) override;
    void Close() override;
    std::string GetIP() override;
    int GetPort() override;
    uint32_t GetConnId() override;
};

} // namespace uv_net

#endif