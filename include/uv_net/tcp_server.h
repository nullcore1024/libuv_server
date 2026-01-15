#ifndef UV_NET_TCP_SERVER_H
#define UV_NET_TCP_SERVER_H

#include "connection.h"
#include "tcp_connection.h"
#include <vector>

namespace uv_net {

// TCP Server
class TcpServer : public Server {
public:
    TcpServer(uv_loop_t* loop);
    ~TcpServer();

    void SetOnOpen(CallbackOpen cb) override { on_open_ = cb; }
    void SetOnMessage(CallbackMessage cb) override { on_message_ = cb; }
    void SetOnClose(CallbackClose cb) override { on_close_ = cb; }

    bool Start(const std::string& ip, int port) override;

    // 内部回调
    void OnNewConnection(std::shared_ptr<Connection> conn);
    void OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len);
    void OnClose(std::shared_ptr<Connection> conn);

private:
    uv_loop_t* loop_;
    std::vector<uv_thread_t> threads_;
    std::vector<uv_loop_t*> loops_;
    std::vector<uv_tcp_t*> listeners_;
    
    CallbackOpen on_open_;
    CallbackMessage on_message_;
    CallbackClose on_close_;
};

} // namespace uv_net

#endif