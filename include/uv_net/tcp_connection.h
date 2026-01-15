#ifndef UV_NET_TCP_CONNECTION_H
#define UV_NET_TCP_CONNECTION_H

#include "connection.h"
#include <queue>
#include <mutex>

namespace uv_net {

class TcpServer;

// TCP 连接实现
class TcpConnection : public Connection {
public:
    uv_tcp_t handle_;
    TcpServer* server_;
    std::string ip_;
    int port_;
    bool is_closing_;

    TcpConnection(TcpServer* server);
    ~TcpConnection() override;

    // 业务调用的 Send
    void Send(const char* data, size_t len) override;
    void Close() override;
    std::string GetIP() override;
    int GetPort() override;

    // 内部逻辑
    void TrySend();
    void OnWriteComplete(int status);

private:
    // 写请求结构体，携带数据缓冲区
    struct WriteReq {
        uv_write_t req;
        std::string data; // 持有数据，防止在回调结束前被释放
    };

    std::queue<std::string> send_queue_;
    bool is_writing_; 
    // 注意：如果 Send 可能被多线程调用，需要加锁。但在 libuv 模型中，
    // 通常建议所有操作都在 Loop 线程进行。如果必须跨线程调用，需要 mutex。
    std::mutex send_mutex_; 
};

} // namespace uv_net

#endif