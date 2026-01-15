#include "uv_net/websocket_server.h"
#include "uv_net/websocket_connection.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h> // for close, SO_REUSEPORT
#include <arpa/inet.h>
#include <plog/Log.h>

namespace uv_net {

static void SetReusePort(uv_handle_t* handle) {
    int fd;
    if (uv_fileno(handle, &fd) == 0) {
        int opt = 1;
#ifdef SO_REUSEPORT
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
    }
}

WebSocketServer::WebSocketServer(uv_loop_t* loop) : loop_(loop) {
    PLOG_INFO << "WebSocket Server created";
}

WebSocketServer::~WebSocketServer() {
    PLOG_INFO << "WebSocket Server destroying";
    for (auto l : listeners_) { uv_close((uv_handle_t*)l, nullptr); delete l; }
    for (auto l : loops_) { if (l != loop_) uv_loop_close(l); delete l; }
    PLOG_INFO << "WebSocket Server destroyed";
}

void WebSocketServer::OnNewConnection(std::shared_ptr<Connection> conn) { 
    PLOG_INFO << "WebSocket Server new connection opened";
    if (on_open_) on_open_(conn); 
}

void WebSocketServer::OnMessage(std::shared_ptr<Connection> conn, const char* data, size_t len) { 
    PLOG_INFO << "WebSocket Server received message of " << len << " bytes";
    if (on_message_) on_message_(conn, data, len); 
}

void WebSocketServer::OnClose(std::shared_ptr<Connection> conn) { 
    PLOG_INFO << "WebSocket Server connection closed";
    if (on_close_) on_close_(conn); 
}

bool WebSocketServer::Start(const std::string& ip, int port, int thread_count) {
    PLOG_INFO << "WebSocket Server starting on " << ip << ":" << port;
    
    // 单线程模式：直接使用传入的loop
    uv_tcp_t* listener = new uv_tcp_t();
    uv_tcp_init(loop_, listener);
    SetReusePort((uv_handle_t*)listener);
    
    struct sockaddr_in addr;
    uv_ip4_addr(ip.c_str(), port, &addr);
    
    if (uv_tcp_bind(listener, (const struct sockaddr*)&addr, 0) != 0) {
        PLOG_ERROR << "WebSocket Server bind failed on " << ip << ":" << port;
        delete listener;
        return false;
    }
    
    listener->data = this;
    
    uv_listen((uv_stream_t*)listener, 128, [](uv_stream_t* server, int status) {
        if (status < 0) {
            PLOG_ERROR << "WebSocket Server listen error: " << uv_strerror(status);
            return;
        }
        WebSocketServer* ws_server = (WebSocketServer*)server->data;
        PLOG_INFO << "WebSocket Server new connection incoming";
        WebSocketConnection* conn = new WebSocketConnection(ws_server);
        uv_tcp_init(server->loop, &conn->handle_);
        
        if (uv_accept(server, (uv_stream_t*)&conn->handle_) == 0) {
            struct sockaddr_storage peer;
            int namelen = sizeof(peer);
            uv_tcp_getpeername(&conn->handle_, (struct sockaddr*)&peer, &namelen);
            conn->ip_ = (peer.ss_family == AF_INET) ? 
                std::string(inet_ntoa(((struct sockaddr_in*)&peer)->sin_addr)) : "Unknown";
            conn->port_ = (peer.ss_family == AF_INET) ? 
                ntohs(((struct sockaddr_in*)&peer)->sin_port) : 0;
            
            PLOG_INFO << "WebSocket Server accepted connection from " << conn->ip_ << ":" << conn->port_;
            
            uv_read_start((uv_stream_t*)&conn->handle_, 
                [](uv_handle_t* h, size_t suggested_size, uv_buf_t* buf) {
                    buf->base = new char[suggested_size];
                    buf->len = suggested_size;
                },
                [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                    WebSocketConnection* conn = (WebSocketConnection*)stream->data;
                    if (nread > 0) {
                        if (conn->state_ == WebSocketConnection::State::HANDSHAKE) {
                            // 处理握手数据
                            conn->handshake_buffer_.append(buf->base, nread);
                            // 检查是否接收到完整的握手请求
                            if (conn->handshake_buffer_.find("\r\n\r\n") != std::string::npos) {
                                conn->ParseHandshake(conn->handshake_buffer_);
                            }
                        } else {
                            // 处理 WebSocket 帧
                            conn->ParseFrame(buf->base, nread);
                        }
                    } else {
                        if (nread != UV_EOF && nread != UV_ECONNRESET) {
                            PLOG_ERROR << "WebSocket Server read error from " << conn->ip_ << ":" << conn->port_ << ": " << uv_strerror(nread);
                        }
                        PLOG_INFO << "WebSocket Server connection closed from " << conn->ip_ << ":" << conn->port_;
                        uv_close((uv_handle_t*)stream, nullptr);
                    }
                    delete[] buf->base;
                }
            );
        } else {
            PLOG_ERROR << "WebSocket Server accept failed";
            uv_close((uv_handle_t*)&conn->handle_, nullptr);
            delete conn;
        }
    });
    
    listeners_.push_back(listener);
    PLOG_INFO << "WebSocket Server started on " << ip << ":" << port;
    return true;
}

} // namespace uv_net