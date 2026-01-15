#ifndef UV_NET_UDP_SERVER_H
#define UV_NET_UDP_SERVER_H

#include "connection.h"
#include "server_config.h"
#include "udp_connection.h"
#include "server_protocol.h"
#include <vector>
#include <memory>

namespace uv_net {

// UDP Server
class UdpServer : public Server {
public:
    UdpServer(uv_loop_t* loop = uv_default_loop(), const ServerConfig& config = ServerConfig());      // 构造函数，默认使用uv_default_loop()
    ~UdpServer();

    void SetOnOpen(CallbackOpen cb) override { on_open_ = cb; }
    void SetOnMessage(CallbackMessage cb) override { on_message_ = cb; }
    void SetOnClose(CallbackClose cb) override { on_close_ = cb; }

    bool Start(const std::string& ip, int port) override;
    void OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len);

    // 缓冲区配置
    void SetReadBufferSize(size_t size) override { config_.SetReadBufferSize(size); }
    void SetMaxSendQueueSize(size_t size) override { config_.SetMaxSendQueueSize(size); }
    
    // 连接池管理
    void SetMaxConnections(size_t max) override { config_.SetMaxConnections(max); }
    void SetHeartbeatInterval(size_t interval_ms) override { config_.SetHeartbeatInterval(interval_ms); }
    
    // 连接超时管理
    void SetConnectionReadTimeout(size_t timeout_ms) override { config_.SetConnectionReadTimeout(timeout_ms); }
    
    // 协议解析器设置
    void SetServerProtocol(std::shared_ptr<ServerProtocol> protocol) override { server_protocol_ = protocol; }
    
    // 获取协议解析器
    std::shared_ptr<ServerProtocol> GetServerProtocol() const { return server_protocol_; }

private:
    uv_loop_t* loop_;                                    // 使用单个loop
    std::vector<uv_udp_t*> sockets_;

    CallbackOpen on_open_;
    CallbackMessage on_message_;
    CallbackClose on_close_;
    
    // 配置
    ServerConfig config_;
    
    // 协议解析器
    std::shared_ptr<ServerProtocol> server_protocol_;
};

} // namespace uv_net

#endif