#ifndef UV_NET_TCP_CONNECTION_H
#define UV_NET_TCP_CONNECTION_H

#include "connection.h"
#include <queue>
#include <mutex>
#include <cstdint>

namespace uv_net {

class TcpServer;

// TCP 连接实现
class TcpConnection : public Connection {
public:
    uv_tcp_t handle_;
    TcpServer* server_;
    std::string ip_;
    int port_;
    uint32_t conn_id_; // 连接ID
    bool is_closing_;
    bool is_closing_gracefully_; // 标记是否正在优雅关闭

    TcpConnection(TcpServer* server);
    ~TcpConnection() override;

    // 业务调用的 Send
    void Send(const char* data, size_t len) override;
    void Close() override;
    std::string GetIP() override;
    int GetPort() override;
    uint32_t GetConnId() override;

    // 内部逻辑
    virtual void TrySend();
    virtual void OnWriteComplete(int status);
    virtual void StartHeartbeat();
    virtual void StopHeartbeat();
    virtual void OnHeartbeatTimeout();
    virtual void OnDataReceived(const char* data, size_t len); // 处理接收到的数据

protected:
    // 写请求结构体，携带数据缓冲区
    struct WriteReq {
        uv_write_t req;
        std::string data; // 持有数据，防止在回调结束前被释放
    };

    std::queue<std::string> send_queue_;
    bool is_writing_;
    std::mutex send_mutex_; // 添加发送锁，用于保护send_queue_和is_writing_
    
    // 心跳相关
    uv_timer_t heartbeat_timer_;
    int64_t last_active_time_; // 上次活跃时间（毫秒）
    int64_t create_time_; // 创建时间（毫秒）
    bool is_heartbeat_running_;
    
    // 接收缓冲区，用于协议解析
    std::vector<char> recv_buffer_;
};

} // namespace uv_net

#endif