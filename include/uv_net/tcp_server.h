#ifndef UV_NET_TCP_SERVER_H
#define UV_NET_TCP_SERVER_H

#include "connection.h"
#include "server_config.h"
#include "tcp_connection.h"
#include "server_protocol.h"
#include "buffer_pool.h"
#include <vector>
#include <atomic>
#include <memory>

namespace uv_net {

// TCP Server
class TcpServer : public Server {
public:
    TcpServer(uv_loop_t* loop, const ServerConfig& config = ServerConfig());
    ~TcpServer();

    void SetOnOpen(CallbackOpen cb) override { on_open_ = cb; }
    void SetOnMessage(CallbackMessage cb) override { on_message_ = cb; }
    void SetOnClose(CallbackClose cb) override { on_close_ = cb; }

    bool Start(const std::string& ip, int port) override;

    // 配置管理
    void SetConfig(const ServerConfig& config) { config_ = config; }
    const ServerConfig& GetConfig() const { return config_; }
    
    // 协议解析器设置
    void SetServerProtocol(std::shared_ptr<ServerProtocol> protocol) { server_protocol_ = protocol; }
    
    // 获取配置（供Connection使用）
    size_t GetReadBufferSize() const { return config_.GetReadBufferSize(); }
    size_t GetWriteBufferSize() const { return config_.GetWriteBufferSize(); }
    size_t GetMaxSendQueueSize() const { return config_.GetMaxSendQueueSize(); }
    size_t GetMaxConnections() const { return config_.GetMaxConnections(); }
    int64_t GetHeartbeatInterval() const { return config_.GetHeartbeatInterval(); }
    int64_t GetConnectionReadTimeout() const { return config_.GetConnectionReadTimeout(); }
    
    // 获取协议解析器
    std::shared_ptr<ServerProtocol> GetServerProtocol() const { return server_protocol_; }

    // 内部回调
    virtual void OnNewConnection(std::shared_ptr<Connection> conn);
    virtual void OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len);
    virtual void OnClose(std::shared_ptr<Connection> conn);
    
    // 创建连接对象的虚函数，供子类重写
    virtual TcpConnection* CreateConnection(TcpServer* server);

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
    
    // 协议解析器
    std::shared_ptr<ServerProtocol> server_protocol_;
    
    // 缓冲区池
    BufferPool buffer_pool_;
    
    // 连接计数
    std::atomic<size_t> current_connections_{0}; // 当前连接数
    std::atomic<uint32_t> conn_id_counter_{0}; // 连接ID计数器
};

} // namespace uv_net

#endif