#ifndef UV_NET_WEBSOCKET_SERVER_H
#define UV_NET_WEBSOCKET_SERVER_H

#include "connection.h"
#include "server_config.h"
#include "websocket_connection.h"
#include <vector>
#include <atomic>

namespace uv_net {

// WebSocket Server
class WebSocketServer : public Server {
public:
    WebSocketServer(uv_loop_t* loop, const ServerConfig& config = ServerConfig());
    ~WebSocketServer();

    void SetOnOpen(CallbackOpen cb) override { on_open_ = cb; }
    void SetOnMessage(CallbackMessage cb) override { on_message_ = cb; }
    void SetOnClose(CallbackClose cb) override { on_close_ = cb; }

    bool Start(const std::string& ip, int port) override;

    // 缓冲区配置
    void SetReadBufferSize(size_t size) override { config_.SetReadBufferSize(size); }
    void SetMaxSendQueueSize(size_t size) override { config_.SetMaxSendQueueSize(size); }
    
    // 连接池管理
    void SetMaxConnections(size_t max) override { config_.SetMaxConnections(max); }
    void SetHeartbeatInterval(size_t interval_ms) override { config_.SetHeartbeatInterval(interval_ms); }
    
    // 连接超时管理
    void SetConnectionReadTimeout(size_t timeout_ms) override { config_.SetConnectionReadTimeout(timeout_ms); }

    // 内部回调
    void OnNewConnection(std::shared_ptr<Connection> conn);
    void OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len);
    void OnClose(std::shared_ptr<Connection> conn);
    
    // 获取配置（供Connection使用）
    const ServerConfig& GetConfig() const;


private:
    uv_loop_t* loop_;
    std::vector<uv_thread_t> threads_;
    std::vector<uv_loop_t*> loops_;
    std::vector<uv_tcp_t*> listeners_;
    
    CallbackOpen on_open_;
    CallbackMessage on_message_;
    CallbackClose on_close_;
    
    // 配置
    ServerConfig config_;
    
    // 连接数统计
    std::atomic<size_t> current_connections_ = {0};
    std::atomic<uint32_t> conn_id_counter_{0}; // 连接ID计数器
};

} // namespace uv_net

#endif