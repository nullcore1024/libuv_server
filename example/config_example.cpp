#include <iostream>
#include <uv.h>
#include "uv_net/tcp_server.h"
#include "uv_net/server_config.h"
#include "Logger.h"

using namespace uv_net;

int main() {
    // 初始化日志
    LogInit(plog::debug,  1<< 26, 7);
    
    // 创建默认事件循环
    uv_loop_t* loop = uv_default_loop();
    
    // 1. 使用默认配置创建服务器
    PLOG_INFO << "=== Example 1: Default Config ===";
    TcpServer server1(loop);
    
    // 2. 使用自定义配置创建服务器
    PLOG_INFO << "=== Example 2: Custom Config ===";
    ServerConfig config;
    config.SetReadBufferSize(16384);           // 16KB读缓冲区
    config.SetWriteBufferSize(16384);          // 16KB写缓冲区
    config.SetMaxConnections(5000);            // 最大5000个连接
    config.SetConnectionReadTimeout(15000);    // 15秒读超时
    config.SetHeartbeatInterval(30000);        // 30秒心跳间隔
    config.SetMaxSendQueueSize(2000);          // 最大发送队列大小2000
    
    TcpServer server2(loop, config);
    
    // 3. 运行时修改配置
    PLOG_INFO << "=== Example 3: Runtime Config Modification ===";
    ServerConfig runtime_config;
    TcpServer server3(loop, runtime_config);
    
    // 运行时修改配置
    server3.SetReadBufferSize(32768);          // 32KB读缓冲区
    server3.SetWriteBufferSize(32768);         // 32KB写缓冲区
    server3.SetMaxConnections(1000);           // 最大1000个连接
    server3.SetConnectionReadTimeout(60000);   // 60秒读超时
    server3.SetHeartbeatInterval(15000);       // 15秒心跳间隔
    
    // 打印配置信息
    PLOG_INFO << "Server 1 Config:";
    PLOG_INFO << "  Read Buffer Size: " << server1.GetReadBufferSize();
    PLOG_INFO << "  Write Buffer Size: " << server1.GetWriteBufferSize();
    PLOG_INFO << "  Max Connections: " << server1.GetMaxConnections();
    PLOG_INFO << "  Connection Read Timeout: " << server1.GetConnectionReadTimeout();
    PLOG_INFO << "  Heartbeat Interval: " << server1.GetHeartbeatInterval();
    
    PLOG_INFO << "\nServer 2 Config:";
    PLOG_INFO << "  Read Buffer Size: " << server2.GetReadBufferSize();
    PLOG_INFO << "  Write Buffer Size: " << server2.GetWriteBufferSize();
    PLOG_INFO << "  Max Connections: " << server2.GetMaxConnections();
    PLOG_INFO << "  Connection Read Timeout: " << server2.GetConnectionReadTimeout();
    PLOG_INFO << "  Heartbeat Interval: " << server2.GetHeartbeatInterval();
    
    PLOG_INFO << "\nServer 3 Config:";
    PLOG_INFO << "  Read Buffer Size: " << server3.GetReadBufferSize();
    PLOG_INFO << "  Write Buffer Size: " << server3.GetWriteBufferSize();
    PLOG_INFO << "  Max Connections: " << server3.GetMaxConnections();
    PLOG_INFO << "  Connection Read Timeout: " << server3.GetConnectionReadTimeout();
    PLOG_INFO << "  Heartbeat Interval: " << server3.GetHeartbeatInterval();
    
    // 启动服务器
    if (server1.Start("0.0.0.0", 8080)) {
        PLOG_INFO << "Server 1 started on 0.0.0.0:8080";
    } else {
        PLOG_ERROR << "Server 1 failed to start";
    }
    
    if (server2.Start("0.0.0.0", 8081)) {
        PLOG_INFO << "Server 2 started on 0.0.0.0:8081";
    } else {
        PLOG_ERROR << "Server 2 failed to start";
    }
    
    if (server3.Start("0.0.0.0", 8082)) {
        PLOG_INFO << "Server 3 started on 0.0.0.0:8082";
    } else {
        PLOG_ERROR << "Server 3 failed to start";
    }
    
    PLOG_INFO << "Config example running. Press Ctrl+C to exit.";
    
    // 运行事件循环
    uv_run(loop, UV_RUN_DEFAULT);
    
    return 0;
}