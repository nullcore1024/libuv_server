#include "uv_net/udp_connection.h"
#include "uv_net/udp_server.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <arpa/inet.h>
#include <plog/Log.h>

namespace uv_net {

UdpConnection::UdpConnection(UdpServer* server, uv_udp_t* socket, const struct sockaddr* addr) 
    : server_(server), socket_(socket), port_(0), conn_id_(0) {
    memcpy(&addr_, addr, sizeof(addr_));
    char ip_buf[64];
    if (addr->sa_family == AF_INET) {
        uv_ip4_name((const sockaddr_in*)addr, ip_buf, 64);
        port_ = ((const sockaddr_in*)addr)->sin_port;
    } else if (addr->sa_family == AF_INET6) {
        uv_ip6_name((const sockaddr_in6*)addr, ip_buf, 64);
        port_ = ((const sockaddr_in6*)addr)->sin6_port;
    }
    ip_ = std::string(ip_buf);
    PLOG_INFO << "UDP Connection created for " << ip_ << ":" << ntohs(port_);
}

void UdpConnection::Send(const char* data, size_t len) {
    PLOG_INFO << "UDP Connection sending " << len << " bytes to " << ip_ << ":" << ntohs(port_);
    uv_buf_t buf = uv_buf_init(new char[len], len);
    memcpy(buf.base, data, len);
    
    uv_udp_send_t* req = new uv_udp_send_t();
    // 存储 buffer 指针以便在回调中释放
    req->data = buf.base;

    uv_udp_send(req, socket_, &buf, 1, (const struct sockaddr*)&addr_, [](uv_udp_send_t* req, int status) {
        delete[] (char*)req->data;
        delete req;
        if (status < 0) {
             PLOG_ERROR << "UDP Send error: " << uv_strerror(status);
        }
    });
}

void UdpConnection::Close() {} // UDP 无连接，无操作
std::string UdpConnection::GetIP() { return ip_; }
int UdpConnection::GetPort() { return ntohs(port_); }
uint32_t UdpConnection::GetConnId() { return conn_id_; }

} // namespace uv_net