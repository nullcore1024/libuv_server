#include <uv_net/tcp_server.h>
#include <uv_net/websocket_server.h>
#include <uv.h>
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <iostream>

using namespace uv_net;

int main() {
    // 初始化日志
    plog::init(plog::debug, "config_socket_example.log");
    PLOG_INFO << "Starting config socket example";

    // 创建默认事件循环
    uv_loop_t* loop = uv_default_loop();

    // 测试 TCP 服务器配置
    {
        PLOG_INFO << "=== Testing TCP Server Config ===";
        
        // 创建配置对象
        ServerConfig config;
        
        // 设置 socket 选项
        config.SetReadBufferSize(16384);      // 16KB 读缓冲区
        config.SetWriteBufferSize(16384);     // 16KB 写缓冲区
        config.SetTcpNoDelay(true);           // 启用 TCP_NODELAY
        
        // 输出配置信息
        PLOG_INFO << "TCP Server Config:";
        PLOG_INFO << "  Read Buffer Size: " << config.GetReadBufferSize();
        PLOG_INFO << "  Write Buffer Size: " << config.GetWriteBufferSize();
        PLOG_INFO << "  TCP_NODELAY: " << (config.GetTcpNoDelay() ? "Enabled" : "Disabled");
        
        // 创建 TCP 服务器
        TcpServer tcp_server(loop, config);
        
        // 启动 TCP 服务器
        if (tcp_server.Start("127.0.0.1", 8080)) {
            PLOG_INFO << "TCP Server started on 127.0.0.1:8080";
        } else {
            PLOG_ERROR << "Failed to start TCP Server";
        }
    }

    // 测试 WebSocket 服务器配置
    {
        PLOG_INFO << "=== Testing WebSocket Server Config ===";
        
        // 创建配置对象
        ServerConfig config;
        
        // 设置不同的 socket 选项
        config.SetReadBufferSize(32768);      // 32KB 读缓冲区
        config.SetWriteBufferSize(32768);     // 32KB 写缓冲区
        config.SetTcpNoDelay(false);          // 禁用 TCP_NODELAY
        
        // 输出配置信息
        PLOG_INFO << "WebSocket Server Config:";
        PLOG_INFO << "  Read Buffer Size: " << config.GetReadBufferSize();
        PLOG_INFO << "  Write Buffer Size: " << config.GetWriteBufferSize();
        PLOG_INFO << "  TCP_NODELAY: " << (config.GetTcpNoDelay() ? "Enabled" : "Disabled");
        
        // 创建 WebSocket 服务器
        WebSocketServer ws_server(loop, config);
        
        // 启动 WebSocket 服务器
        if (ws_server.Start("127.0.0.1", 8081)) {
            PLOG_INFO << "WebSocket Server started on 127.0.0.1:8081";
        } else {
            PLOG_ERROR << "Failed to start WebSocket Server";
        }
    }

    PLOG_INFO << "Example completed, press Ctrl+C to exit";
    
    // 运行事件循环
    uv_run(loop, UV_RUN_DEFAULT);
    
    return 0;
}