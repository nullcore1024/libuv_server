#include "uv_net/websocket_server.h"
#include "uv_net/websocket_connection.h"
#include <plog/Log.h>

namespace uv_net {

WebSocketServer::WebSocketServer(uv_loop_t* loop, const ServerConfig& config) : TcpServer(loop, config) {
    PLOG_INFO << "WebSocket Server created with buffer pool size: " << config.GetReadBufferSize();
}

WebSocketServer::~WebSocketServer() {
    PLOG_INFO << "WebSocket Server destroying";
    // 基类的析构函数会处理资源释放
    PLOG_INFO << "WebSocket Server destroyed";
}

void WebSocketServer::OnNewConnection(std::shared_ptr<Connection> conn) {
    PLOG_INFO << "WebSocket Server new connection established";
    // 调用基类的OnNewConnection，它会处理连接计数和回调
    TcpServer::OnNewConnection(conn);
}

void WebSocketServer::OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len) {
    PLOG_INFO << "WebSocket Server received message of " << len << " bytes";
    // 调用基类的OnMessage，它会处理回调
    TcpServer::OnMessage(conn, data, len);
}

void WebSocketServer::OnClose(std::shared_ptr<Connection> conn) {
    PLOG_INFO << "WebSocket Server connection closed";
    // 调用基类的OnClose，它会处理连接计数和回调
    TcpServer::OnClose(conn);
}

// 重写父类的CreateConnection方法，创建WebSocketConnection对象
TcpConnection* WebSocketServer::CreateConnection(TcpServer* server) {
    // 将TcpServer指针转换为WebSocketServer指针
    WebSocketServer* ws_server = static_cast<WebSocketServer*>(server);
    return new WebSocketConnection(ws_server);
}

} // namespace uv_net