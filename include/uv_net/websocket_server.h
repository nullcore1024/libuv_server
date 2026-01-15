#ifndef UV_NET_WEBSOCKET_SERVER_H
#define UV_NET_WEBSOCKET_SERVER_H

#include "connection.h"
#include "server_config.h"
#include "websocket_connection.h"
#include "server_protocol.h"
#include <vector>
#include <atomic>
#include <memory>

namespace uv_net {

// WebSocket Server
class WebSocketServer : public Server {
    friend class WebSocketConnection;
public:
    WebSocketServer(uv_loop_t* loop, const ServerConfig& config = ServerConfig());
    ~WebSocketServer();

    void SetOnOpen(CallbackOpen cb) override { on_open_ = cb; }
    void SetOnMessage(CallbackMessage cb) override { on_message_ = cb; }
    void SetOnClose(CallbackClose cb) override { on_close_ = cb; }

    bool Start(const std::string& ip, int port) override;

    // 协议解析器设置
    void SetServerProtocol(std::shared_ptr<ServerProtocol> protocol) { server_protocol_ = protocol; }
    
    // 获取协议解析器
    std::shared_ptr<ServerProtocol> GetServerProtocol() const { return server_protocol_; }

    // 获取配置（供Connection使用）
    const ServerConfig& GetConfig() const;

private:
    // 内部回调
    void OnNewConnection(std::shared_ptr<Connection> conn);
    void OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len);
    void OnClose(std::shared_ptr<Connection> conn);
    
    uv_loop_t* loop_;
    std::vector<uv_thread_t> threads_;
    std::vector<uv_loop_t*> loops_;
    std::vector<uv_tcp_t*> listeners_;
    
    CallbackOpen on_open_;
    CallbackMessage on_message_;
    CallbackClose on_close_;
    
    // 配置
    ServerConfig config_;
    
    // 协议解析器
    std::shared_ptr<ServerProtocol> server_protocol_;
    
    // 连接数统计
    std::atomic<size_t> current_connections_ = {0};
    std::atomic<uint32_t> conn_id_counter_{0}; // 连接ID计数器
};

} // namespace uv_net

#endif