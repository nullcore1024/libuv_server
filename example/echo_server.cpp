#include "uv_net.h"
#include "Logger.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace uv_net;

// 简单的 Echo 测试函数
void TestHighThroughput(std::shared_ptr<Connection> conn) {
    // 模拟高频发送：循环发送 1000 次 "Hello"
    // 由于我们有发送队列，这不会阻塞，也不会导致 libuv 报错
    for (int i = 0; i < 100; ++i) {
        std::string msg = "Bulk " + std::to_string(i) + "\n";
        conn->Send(msg.c_str(), msg.size());
    }
}

int main() {
    // 初始化plog
    LogInit(plog::debug, 1<<26, 7);
    
    PLOG_DEBUG << "--- Echo Server (Single Thread Mode) ---";

    uv_loop_t* loop = uv_default_loop();
    TcpServer tcp_server(loop);
    WebSocketServer ws_server(loop);

    int connection_count = 0;

    tcp_server.SetOnOpen([&connection_count](std::shared_ptr<Connection> conn) {
        connection_count++;
        PLOG_DEBUG << "[Open] Client: " << conn->GetIP() << ":" << conn->GetPort() 
                  << " (Total: " << connection_count << ")";
        
        // 发送欢迎消息
        conn->Send("Welcome! Type something.\n", 22);
    });

    tcp_server.SetOnMessage([](std::shared_ptr<Connection> conn, const char* data, size_t len) {
        std::string msg(data, len);
        PLOG_DEBUG << "[Msg] From " << conn->GetIP() << ": " << msg;

        // Echo back
        conn->Send(data, len);

        // 如果收到 "bulk"，测试高并发发送
        if (msg.find("bulk") != std::string::npos) {
            PLOG_DEBUG << "[Test] Sending bulk data...";
            TestHighThroughput(conn);
        }
    });

    tcp_server.SetOnClose([&connection_count](std::shared_ptr<Connection> conn) {
        connection_count--;
        PLOG_DEBUG << "[Close] Client: " << conn->GetIP() << ":" << conn->GetPort() 
                  << " (Total: " << connection_count << ")";
    });

    // 启动 TCP，使用单线程模式
    if (!tcp_server.Start("0.0.0.0", 7000)) {
        PLOG_ERROR << "Failed to start TCP server";
        return 1;
    }

    // 启动 UDP，使用与TCP相同的loop，单线程模式
    UdpServer udp_server(loop);
    udp_server.SetOnMessage([](std::shared_ptr<Connection> conn, const char* data, size_t len) {
        PLOG_DEBUG << "[UDP] " << conn->GetIP() << ":" << conn->GetPort() << " -> " << std::string(data, len);
        conn->Send(data, len);
    });
    if (!udp_server.Start("0.0.0.0", 7001)) {
        PLOG_ERROR << "Failed to start UDP server";
    }

    // WebSocket Server：使用与TCP相同的loop，单线程模式
    ws_server.SetOnOpen([&connection_count](std::shared_ptr<Connection> conn) {
        connection_count++;
        PLOG_DEBUG << "[WS-Open] Client: " << conn->GetIP() << ":" << conn->GetPort() 
                  << " (Total: " << connection_count << ")";
        
        // 发送欢迎消息
        conn->Send("Welcome to WebSocket Echo Server!", 30);
    });

    ws_server.SetOnMessage([](std::shared_ptr<Connection> conn, const char* data, size_t len) {
        std::string msg(data, len);
        PLOG_DEBUG << "[WS-Msg] From " << conn->GetIP() << ": " << msg;

        // Echo back
        conn->Send(data, len);
    });

    ws_server.SetOnClose([&connection_count](std::shared_ptr<Connection> conn) {
        connection_count--;
        PLOG_DEBUG << "[WS-Close] Client: " << conn->GetIP() << ":" << conn->GetPort() 
                  << " (Total: " << connection_count << ")";
    });

    if (!ws_server.Start("0.0.0.0", 8080)) {
        PLOG_ERROR << "Failed to start WebSocket server";
    }

    PLOG_DEBUG << "Server running on TCP 7000, UDP 7001, WebSocket 8080 (Single Thread Mode).";
    PLOG_DEBUG << "Type 'bulk' in TCP client to test send buffering.";
    PLOG_DEBUG << "Use WebSocket client to connect to ws://0.0.0.0:8080";
    PLOG_DEBUG << "Press Enter to exit...";

    // 创建一个异步句柄来处理用户输入
    uv_async_t async;
    uv_async_init(loop, &async, [](uv_async_t* handle) {
        // 用户按下了Enter键，停止事件循环
        uv_stop(handle->loop);
    });

    // 在另一个线程中等待用户输入
    std::thread([&async]() {
        std::cin.get();
        // 通知事件循环退出
        uv_async_send(&async);
    }).detach();

    // 运行事件循环（TCP和UDP服务器都依赖它）
    uv_run(loop, UV_RUN_DEFAULT);

    // 清理异步句柄
    uv_close((uv_handle_t*)&async, nullptr);
    uv_run(loop, UV_RUN_ONCE);

    return 0;
}
