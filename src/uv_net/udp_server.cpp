#include "uv_net/udp_server.h"
#include "uv_net/udp_connection.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <plog/Log.h>

namespace uv_net {

UdpServer::UdpServer(uv_loop_t* loop, const ServerConfig& config) : loop_(loop), config_(config) {
    PLOG_INFO << "UDP Server created";
} // 构造函数，接受loop和config参数

void UdpServer::OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len) {
    if (on_message_) {
        PLOG_INFO << "UDP Server received message of " << len << " bytes";
        on_message_(conn, data, len);
    }
}

UdpServer::~UdpServer() {
    PLOG_INFO << "UDP Server destroying";
    for (auto s : sockets_) { 
        uv_close((uv_handle_t*)s, nullptr); 
        delete s; 
    }
    // 不关闭loop，因为loop是外部传入的
    PLOG_INFO << "UDP Server destroyed";
}

bool UdpServer::Start(const std::string& ip, int port) {
    PLOG_INFO << "UDP Server starting on " << ip << ":" << port;
    
    // 单线程模式：直接使用传入的loop
    uv_udp_t* socket = new uv_udp_t();
    uv_udp_init(loop_, socket);
    
    struct sockaddr_in addr;
    uv_ip4_addr(ip.c_str(), port, &addr);
    
    if (uv_udp_bind(socket, (const struct sockaddr*)&addr, UV_UDP_REUSEADDR) != 0) {
        PLOG_ERROR << "UDP Server bind failed on " << ip << ":" << port;
        delete socket;
        return false;
    }
    
    socket->data = this;
    
    uv_udp_recv_start(socket, 
        [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
            buf->base = new char[suggested_size];
            buf->len = suggested_size;
        },
        [](uv_udp_t* socket, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
            if (nread > 0 && addr) {
                UdpServer* server = (UdpServer*)socket->data;
                PLOG_INFO << "UDP Server received " << nread << " bytes";
                std::shared_ptr<UdpConnection> conn = std::make_shared<UdpConnection>(server, socket, addr);
                server->OnMessage(conn, buf->base, nread);
            } else if (nread < 0) {
                PLOG_ERROR << "UDP Server recv error: " << uv_strerror(nread);
            }
            delete[] buf->base;
        }
    );
    
    sockets_.push_back(socket);
    PLOG_INFO << "UDP Server started on " << ip << ":" << port;
    return true;
}

} // namespace uv_net