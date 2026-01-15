#ifndef UV_NET_WEBSOCKET_SERVER_H
#define UV_NET_WEBSOCKET_SERVER_H

#include "connection.h"
#include "server_config.h"
#include "websocket_connection.h"
#include "server_protocol.h"
#include "buffer_pool.h"
#include "tcp_server.h"
#include <vector>
#include <atomic>
#include <memory>

namespace uv_net {

// WebSocket Server
class WebSocketServer : public TcpServer {
    friend class WebSocketConnection;
public:
    WebSocketServer(uv_loop_t* loop, const ServerConfig& config = ServerConfig());
    ~WebSocketServer();

    // WebSocket特有的方法可以在这里添加

private:
    // 内部回调
    void OnNewConnection(std::shared_ptr<Connection> conn) override;
    void OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len) override;
    void OnClose(std::shared_ptr<Connection> conn) override;
    
    // 重写父类的CreateConnection方法，创建WebSocketConnection对象
    TcpConnection* CreateConnection(TcpServer* server) override;
};

} // namespace uv_net

#endif