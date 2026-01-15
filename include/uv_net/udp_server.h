#ifndef UV_NET_UDP_SERVER_H
#define UV_NET_UDP_SERVER_H

#include "connection.h"
#include "udp_connection.h"
#include <vector>

namespace uv_net {

// UDP Server
class UdpServer : public Server {
public:
    UdpServer(uv_loop_t* loop = uv_default_loop());      // 构造函数，默认使用uv_default_loop()
    ~UdpServer();

    void SetOnOpen(CallbackOpen cb) override { on_open_ = cb; }
    void SetOnMessage(CallbackMessage cb) override { on_message_ = cb; }
    void SetOnClose(CallbackClose cb) override { on_close_ = cb; }

    bool Start(const std::string& ip, int port, int thread_count) override;
    void OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len);

private:
    uv_loop_t* loop_;                                    // 使用单个loop
    std::vector<uv_udp_t*> sockets_;

    CallbackOpen on_open_;
    CallbackMessage on_message_;
    CallbackClose on_close_;
};

} // namespace uv_net

#endif