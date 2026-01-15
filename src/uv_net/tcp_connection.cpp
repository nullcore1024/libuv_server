#include "uv_net/tcp_connection.h"
#include "uv_net/tcp_server.h"
#include "server_protocol.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <plog/Log.h>

namespace uv_net {

TcpConnection::TcpConnection(TcpServer* server) 
    : server_(server), port_(0), conn_id_(0), is_closing_(false), is_closing_gracefully_(false), is_writing_(false), is_heartbeat_running_(false) {
    handle_.data = this;
    // 初始化心跳定时器
    uv_timer_init(uv_default_loop(), &heartbeat_timer_);
    heartbeat_timer_.data = this;
    // 记录创建时间
    create_time_ = uv_now(uv_default_loop());
    last_active_time_ = create_time_;
    PLOG_INFO << "TCP Connection created";
}

TcpConnection::~TcpConnection() {
    // 析构函数不做清理，因为 handle 的清理由 Close 触发
}

void TcpConnection::Send(const char* data, size_t len) {
    // 如果正在关闭，直接丢弃
    if (is_closing_ || is_closing_gracefully_) {
        PLOG_INFO << "TCP Connection " << conn_id_ << " is closing, dropping send request";
        return;
    }

    // 检查发送队列大小是否超过配置的最大值
    if (send_queue_.size() >= server_->GetMaxSendQueueSize()) {
        PLOG_WARNING << "TCP Connection " << conn_id_ << " send queue full, dropping send request";
        return;
    }
    send_queue_.emplace(data, len);
    
    PLOG_INFO << "TCP Connection " << conn_id_ << " send queued " << len << " bytes";
    
    // 更新最后活跃时间
    last_active_time_ = uv_now(uv_default_loop());
    
    // 触发发送尝试
    TrySend();
}

void TcpConnection::TrySend() {
    std::string data_to_send;
    // 如果正在发送，或者队列为空，直接返回
    if (is_writing_ || send_queue_.empty()) {
        return;
    }
    
    // 取出队首数据
    data_to_send = std::move(send_queue_.front());
    send_queue_.pop();
    
    // 标记正在发送
    is_writing_ = true;

    PLOG_INFO << "TCP Connection " << conn_id_ << " sending " << data_to_send.size() << " bytes";

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
        PLOG_ERROR << "TCP Connection " << conn_id_ << " send failed: " << uv_strerror(r);
        delete req; // 回调没触发，手动删
        is_writing_ = false; // 恢复状态
        // 错误发生，关闭连接
        Close();
    }
}

void TcpConnection::OnWriteComplete(int status) {
    if (status < 0) {
        if (status != UV_ECANCELED) { // 主动关闭也会产生 ECANCELED，不算错误
            PLOG_ERROR << "TCP Connection " << conn_id_ << " write failed: " << uv_strerror(status);
            Close();
        }
        return;
    }

    PLOG_INFO << "TCP Connection " << conn_id_ << " write complete";

    // 写成功，重置标志
    is_writing_ = false;

    // 更新最后活跃时间
    last_active_time_ = uv_now(uv_default_loop());

    // *** 关键：尝试发送队列中的下一包数据 ***
    TrySend();
    
    // 检查是否需要优雅关闭
    if (is_closing_gracefully_ && send_queue_.empty() && !is_writing_) {
        PLOG_INFO << "TCP Connection " << conn_id_ << " send queue empty, closing gracefully";
        // 发送队列已空，执行实际关闭
        uv_close((uv_handle_t*)&handle_, [](uv_handle_t* handle) {
            TcpConnection* conn = static_cast<TcpConnection*>(handle->data);
            // 计算在线时长（秒）
            size_t now = uv_now(uv_default_loop());
            double online_seconds = (now - conn->create_time_) / 1000.0;
            PLOG_INFO << "TCP Connection " << conn->conn_id_ << " closed gracefully, online time: " << online_seconds << " seconds";
            // 触发用户层的 OnClose
            if (conn->server_) {
                conn->server_->OnClose(std::shared_ptr<TcpConnection>(conn, [](TcpConnection*){}));
            }
            delete conn; // 最终释放 Connection 对象
        });
    }
}

void TcpConnection::Close() {
    if (is_closing_ || is_closing_gracefully_) {
        return;
    }
    
    // 停止心跳
    StopHeartbeat();
    
    // 检查发送队列是否为空
    bool is_empty = send_queue_.empty();
    
    if (is_empty && !is_writing_) {
        // 发送队列为空，直接关闭
        is_closing_ = true;
        PLOG_INFO << "TCP Connection " << conn_id_ << " closing immediately";
        
        // 关闭 handle，触发 close 回调
        uv_close((uv_handle_t*)&handle_, [](uv_handle_t* handle) {
                TcpConnection* conn = static_cast<TcpConnection*>(handle->data);
                // 计算在线时长（秒）
                size_t now = uv_now(uv_default_loop());
                double online_seconds = (now - conn->create_time_) / 1000.0;
                PLOG_INFO << "TCP Connection " << conn->conn_id_ << " closed immediately, online time: " << online_seconds << " seconds";
                // 触发用户层的 OnClose
                if (conn->server_) {
                    conn->server_->OnClose(std::shared_ptr<TcpConnection>(conn, [](TcpConnection*){}));
                }
                delete conn; // 最终释放 Connection 对象
            });
    } else {
        // 发送队列不为空，执行优雅关闭
        is_closing_gracefully_ = true;
        PLOG_INFO << "TCP Connection " << conn_id_ << " closing gracefully";
        // 不立即关闭，等待发送队列处理完毕
    }
}

void TcpConnection::StartHeartbeat() {
    if (is_heartbeat_running_) {
        return;
    }
    
    is_heartbeat_running_ = true;
    last_active_time_ = uv_now(uv_default_loop());
    
    // 启动心跳定时器，间隔为配置的心跳间隔
    uv_timer_start(&heartbeat_timer_, [](uv_timer_t* timer) {
        TcpConnection* conn = static_cast<TcpConnection*>(timer->data);
        conn->OnHeartbeatTimeout();
    }, server_->GetHeartbeatInterval(), server_->GetHeartbeatInterval());
    
    PLOG_INFO << "TCP Connection " << conn_id_ << " heartbeat started, interval: " << server_->GetHeartbeatInterval() << "ms";
}

void TcpConnection::StopHeartbeat() {
    if (!is_heartbeat_running_) {
        return;
    }
    
    uv_timer_stop(&heartbeat_timer_);
    is_heartbeat_running_ = false;
    PLOG_INFO << "TCP Connection " << conn_id_ << " heartbeat stopped";
}

void TcpConnection::OnHeartbeatTimeout() {
    size_t now = uv_now(uv_default_loop());
    size_t interval = server_->GetHeartbeatInterval();
    
    // 检查上次活跃时间是否超过心跳间隔的两倍
    if (now - last_active_time_ > interval * 2) {
        PLOG_WARNING << "TCP Connection " << conn_id_ << " heartbeat timeout, closing connection";
        Close();
    }
    // 否则，继续等待下一次心跳检查
}

std::string TcpConnection::GetIP() { return ip_; }
int TcpConnection::GetPort() { return port_; }
uint32_t TcpConnection::GetConnId() { return conn_id_; }

void TcpConnection::OnDataReceived(const char* data, size_t len) {
    // 更新最后活跃时间
    last_active_time_ = uv_now(uv_default_loop());
    
    // 将新数据添加到接收缓冲区
    recv_buffer_.insert(recv_buffer_.end(), data, data + len);
    
    // 获取协议解析器
    auto protocol = server_->GetServerProtocol();
    
    // 如果没有配置协议解析器，直接调用OnMessage
    if (!protocol) {
        server_->OnMessage(std::shared_ptr<TcpConnection>(this, [](TcpConnection*){}), recv_buffer_.data(), recv_buffer_.size());
        recv_buffer_.clear();
        return;
    }
    
    // 使用协议解析器解析数据
    while (!recv_buffer_.empty()) {
        int package_len = 0;
        int msg_len = 0;
        
        // 调用协议解析器解析包
        PackageStatus status = protocol->ParsePackage(recv_buffer_.data(), recv_buffer_.size(), package_len, msg_len);
        
        if (status == PackageFull) {
            // 完整包，调用OnMessage
            server_->OnMessage(std::shared_ptr<TcpConnection>(this, [](TcpConnection*){}), recv_buffer_.data(), package_len);
            
            // 移除已处理的数据
            recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + package_len);
        } else if (status == PackageLess) {
            // 数据不足，等待更多数据
            break;
        } else {
            // 包错误，关闭连接
            PLOG_ERROR << "TCP Connection " << conn_id_ << " package parse error, closing connection";
            Close();
            break;
        }
    }
}

} // namespace uv_net