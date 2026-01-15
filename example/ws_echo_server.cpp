#include "uv_net.h"
#include "Logger.h"
#include <iostream>
#include <thread>

using namespace uv_net;

int main() {
    // 初始化plog
    LogInit(plog::debug, 1<<26, 7);
    
    PLOG_DEBUG << "--- WebSocket Echo Server ---";

    uv_loop_t* loop = uv_default_loop();
    WebSocketServer ws_server(loop);

    int connection_count = 0;

    ws_server.SetOnOpen([&connection_count](std::shared_ptr<Connection> conn) {
        connection_count++;
        PLOG_DEBUG << "[Open] Client: " << conn->GetIP() << ":" << conn->GetPort() 
                  << " (Total: " << connection_count << ")";
        
        // 发送欢迎消息
        conn->Send("Welcome to WebSocket Echo Server!", 30);
    });

    ws_server.SetOnMessage([](std::shared_ptr<Connection> conn, const char* data, size_t len) {
        std::string msg(data, len);
        PLOG_DEBUG << "[Msg] From " << conn->GetIP() << ": " << msg;

        // Echo back
        conn->Send(data, len);
    });

    ws_server.SetOnClose([&connection_count](std::shared_ptr<Connection> conn) {
        connection_count--;
        PLOG_DEBUG << "[Close] Client: " << conn->GetIP() << ":" << conn->GetPort() 
                  << " (Total: " << connection_count << ")";
    });

    // 启动 WebSocket 服务器
    if (!ws_server.Start("0.0.0.0", 8080)) {
        PLOG_ERROR << "Failed to start WebSocket server";
        return 1;
    }

    PLOG_DEBUG << "WebSocket server running on ws://0.0.0.0:8080";
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

    // 运行事件循环
    uv_run(loop, UV_RUN_DEFAULT);

    // 清理异步句柄
    uv_close((uv_handle_t*)&async, nullptr);
    uv_run(loop, UV_RUN_ONCE);

    return 0;
}