#include "uv_net/tcp_connection.h"
#include "uv_net/tcp_server.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <plog/Log.h>

namespace uv_net {

TcpConnection::TcpConnection(TcpServer* server) 
    : server_(server), port_(0), is_closing_(false), is_writing_(false) {
    handle_.data = this;
    PLOG_INFO << "TCP Connection created";
}

TcpConnection::~TcpConnection() {
    // 析构函数不做清理，因为 handle 的清理由 Close 触发
}

void TcpConnection::Send(const char* data, size_t len) {
    // 如果正在关闭，直接丢弃
    if (is_closing_) {
        PLOG_INFO << "TCP Connection is closing, dropping send request";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        send_queue_.emplace(data, len);
    }
    
    PLOG_INFO << "TCP Connection send queued " << len << " bytes";
    
    // 触发发送尝试
    // 注意：这里假设 Send 是在 Loop 线程调用的。如果是跨线程，需要 uv_async
    TrySend();
}

void TcpConnection::TrySend() {
    std::string data_to_send;
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        // 如果正在发送，或者队列为空，直接返回
        if (is_writing_ || send_queue_.empty()) {
            return;
        }
        
        // 取出队首数据
        data_to_send = std::move(send_queue_.front());
        send_queue_.pop();
        
        // 标记正在发送
        is_writing_ = true;
    }

    PLOG_INFO << "TCP Connection sending " << data_to_send.size() << " bytes";

    // 准备 libuv 写请求
    WriteReq* req = new WriteReq();
    // 将数据移动到 req 中，保证异步回调期间数据有效
    req->data = std::move(data_to_send);
    req->req.data = this;

    uv_buf_t buf = uv_buf_init(const_cast<char*>(req->data.c_str()), req->data.size());

    int r = uv_write(&req->req, (uv_stream_t*)&handle_, &buf, 1, 
        [](uv_write_t* uv_req, int status) {
            // 使用 reinterpret_cast 而不是 static_cast
            WriteReq* wr = reinterpret_cast<WriteReq*>(uv_req);
            TcpConnection* conn = static_cast<TcpConnection*>(wr->req.data);
            
            // 1. 释放请求内存（连带释放 string buffer）
            delete wr;

            // 2. 处理回调状态
            conn->OnWriteComplete(status);
        }
    );

    if (r != 0) {
        PLOG_ERROR << "TCP Connection send failed: " << uv_strerror(r);
        delete req; // 回调没触发，手动删
        is_writing_ = false; // 恢复状态
        // 错误发生，关闭连接
        Close();
    }
}

void TcpConnection::OnWriteComplete(int status) {
    if (status < 0) {
        if (status != UV_ECANCELED) { // 主动关闭也会产生 ECANCELED，不算错误
            PLOG_ERROR << "TCP Connection write failed: " << uv_strerror(status);
            Close();
        }
        return;
    }

    PLOG_INFO << "TCP Connection write complete";

    // 写成功，重置标志
    is_writing_ = false;

    // *** 关键：尝试发送队列中的下一包数据 ***
    TrySend();
}

void TcpConnection::Close() {
    if (is_closing_) return;
    is_closing_ = true;
    
    PLOG_INFO << "TCP Connection closing";

    // 关闭 handle，触发 close 回调
    uv_close((uv_handle_t*)&handle_, [](uv_handle_t* handle) {
        TcpConnection* conn = static_cast<TcpConnection*>(handle->data);
        PLOG_INFO << "TCP Connection closed";
        // 触发用户层的 OnClose
        if (conn->server_) {
            // 这里创建一个 shared_ptr 但不做任何资源管理，仅用于传递给用户
            // 实际内存由 close 回调里的 delete conn 负责
            // 注意：这是一个简化的处理，复杂场景需要 shared_from_this
            conn->server_->OnClose(std::shared_ptr<TcpConnection>(conn, [](TcpConnection*){}));
        }
        delete conn; // 最终释放 Connection 对象
    });
}

std::string TcpConnection::GetIP() { return ip_; }
int TcpConnection::GetPort() { return port_; }

} // namespace uv_net