#ifndef UV_NET_CONNECTION_H
#define UV_NET_CONNECTION_H

#include <uv.h>
#include <functional>
#include <memory>
#include <string>
#include <plog/Log.h>
#include "server_protocol.h"

namespace uv_net {

// 前向声明
class TcpServer;
class UdpServer;
class WebSocketServer;

// 回调类型定义
using CallbackOpen = std::function<void(std::shared_ptr<class Connection>)>;
using CallbackMessage = std::function<void(std::shared_ptr<class Connection>, const char* data, size_t len)>;
using CallbackClose = std::function<void(std::shared_ptr<class Connection>)>;

// 抽象连接类
class Connection {
public:
    virtual ~Connection() = default;
    virtual void Send(const char* data, size_t len) = 0;
    virtual void Close() = 0;
    virtual std::string GetIP() = 0;
    virtual int GetPort() = 0;
    virtual uint32_t GetConnId() = 0;
};

// 基础 Server 接口
class Server {
public:
    virtual ~Server() = default;
    virtual void SetOnOpen(CallbackOpen cb) = 0;
    virtual void SetOnMessage(CallbackMessage cb) = 0;
    virtual void SetOnClose(CallbackClose cb) = 0;
    virtual bool Start(const std::string& ip, int port) = 0;
};

} // namespace uv_net

#endif