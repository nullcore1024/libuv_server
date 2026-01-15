#include "uv_net/websocket_connection.h"
#include "uv_net/websocket_server.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h> // for SHA-1
#include <openssl/bio.h> // for BIO functions
#include <openssl/evp.h> // for EVP functions
#include <plog/Log.h>

namespace uv_net {

static std::string Base64Encode(const unsigned char* data, size_t len) {
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
            
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];
            
        while((i++ < 3))
            ret += '=';
    }
    
    return ret;
}

WebSocketConnection::WebSocketConnection(WebSocketServer* server)
    : TcpConnection(server), state_(State::HANDSHAKE),
      parse_state_(ParseState::READ_HEADER), current_frame_(),
      bytes_read_(0) {
    // 将服务器指针转换为WebSocketServer类型
    server_ = server;
    
    memset(&current_frame_, 0, sizeof(current_frame_));
    
    PLOG_INFO << "WebSocket Connection created";
}

WebSocketConnection::~WebSocketConnection() {
    PLOG_INFO << "WebSocket Connection destroyed";
    // 析构函数不做清理，因为 handle 的清理由 Close 触发
}

// WebSocket 握手相关方法
std::string WebSocketConnection::GenerateResponseKey(const std::string& sec_websocket_key) {
    const std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = sec_websocket_key + magic_string;
    
    // 计算 SHA-1
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), sha1_hash);
    
    // 进行 Base64 编码
    std::string response_key = Base64Encode(sha1_hash, SHA_DIGEST_LENGTH);
    
    return response_key;
}

bool WebSocketConnection::ParseHandshake(const std::string& handshake_data) {
    PLOG_INFO << "WebSocket Connection parsing handshake";
    // 简单的握手解析，提取 Sec-WebSocket-Key
    size_t key_pos = handshake_data.find("Sec-WebSocket-Key: ");
    if (key_pos == std::string::npos) {
        PLOG_ERROR << "WebSocket Connection handshake failed: missing Sec-WebSocket-Key";
        return false;
    }
    
    size_t key_end = handshake_data.find("\r\n", key_pos);
    if (key_end == std::string::npos) {
        PLOG_ERROR << "WebSocket Connection handshake failed: invalid Sec-WebSocket-Key format";
        return false;
    }
    
    std::string sec_websocket_key = handshake_data.substr(key_pos + 19, key_end - (key_pos + 19));
    PLOG_INFO << "WebSocket Connection sending handshake response";
    SendHandshakeResponse(sec_websocket_key);
    return true;
}

void WebSocketConnection::SendHandshakeResponse(const std::string& sec_websocket_key) {
    std::string response_key = GenerateResponseKey(sec_websocket_key);
    
    std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + response_key + "\r\n\r\n";
    
    // 发送握手响应
    uv_buf_t buf = uv_buf_init(const_cast<char*>(response.c_str()), response.size());
    uv_write_t* req = new uv_write_t();
    req->data = this;
    
    uv_write(req, (uv_stream_t*)&handle_, &buf, 1, [](uv_write_t* uv_req, int status) {
        WebSocketConnection* conn = static_cast<WebSocketConnection*>(uv_req->data);
        delete uv_req;
        conn->OnHandshakeComplete();
    });
}

void WebSocketConnection::OnHandshakeComplete() {
    state_ = State::OPEN;
    PLOG_INFO << "WebSocket Connection handshake completed, connection open";
    
    // 启动心跳机制
    StartHeartbeat();
    
    // 触发用户层的 OnOpen
    if (server_) {
        std::shared_ptr<WebSocketConnection> shared_conn(this, [](WebSocketConnection*){});
        server_->OnNewConnection(shared_conn);
    }
}

// WebSocket 帧处理方法
void WebSocketConnection::SendFrame(const char* data, size_t len, uint8_t opcode) {
    std::string frame;
    
    // 第1字节：FIN + RSV + OPCODE
    uint8_t first_byte = (1 << 7) | (opcode & 0x0F);
    frame.push_back(first_byte);
    
    // 第2字节：MASK + PAYLOAD_LENGTH
    uint8_t second_byte = 0; // 服务器发送的数据不使用掩码
    
    if (len < 126) {
        second_byte |= static_cast<uint8_t>(len);
        frame.push_back(second_byte);
    } else if (len < 65536) {
        second_byte |= 126;
        frame.push_back(second_byte);
        uint16_t len16 = htons(static_cast<uint16_t>(len));
        frame.append(reinterpret_cast<const char*>(&len16), 2);
    } else {
        second_byte |= 127;
        frame.push_back(second_byte);
        uint64_t len64 = htobe64(static_cast<uint64_t>(len));
        frame.append(reinterpret_cast<const char*>(&len64), 8);
    }
    
    // 有效负载数据
    frame.append(data, len);
    
    // 发送帧
    {    std::lock_guard<std::mutex> lock(send_mutex_);
        send_queue_.push(frame);
    }
    
    // 触发发送尝试
    TrySend();
}

void WebSocketConnection::TrySend() {
    std::string data_to_send;
    {    std::lock_guard<std::mutex> lock(send_mutex_);
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
            WebSocketConnection* conn = static_cast<WebSocketConnection*>(wr->req.data);
            
            // 1. 释放请求内存（连带释放 string buffer）
            delete wr;
            
            // 2. 处理回调状态
            conn->OnWriteComplete(status);
        }
    );
    
    if (r != 0) {
        std::cerr << "Send error: " << uv_strerror(r) << std::endl;
        delete req; // 回调没触发，手动删
        is_writing_ = false; // 恢复状态
        // 错误发生，关闭连接
        Close();
    }
}

void WebSocketConnection::ParseFrame(const char* data, size_t len) {
    PLOG_INFO << "WebSocket Connection parsing frame of " << len << " bytes";
    const char* ptr = data;
    const char* end = data + len;
    
    while (ptr < end) {
        switch (parse_state_) {
            case ParseState::READ_HEADER: {
                if (bytes_read_ < 2) {
                    buffer_.insert(buffer_.end(), ptr, end);
                    bytes_read_ += (end - ptr);
                    ptr = end;
                    
                    if (bytes_read_ >= 2) {
                        // 解析头部
                        uint8_t first_byte = static_cast<uint8_t>(buffer_[0]);
                        uint8_t second_byte = static_cast<uint8_t>(buffer_[1]);
                        
                        current_frame_.fin = (first_byte & 0x80) != 0;
                        current_frame_.opcode = first_byte & 0x0F;
                        current_frame_.masked = (second_byte & 0x80) != 0;
                        
                        uint8_t payload_len = second_byte & 0x7F;
                        if (payload_len < 126) {
                            current_frame_.payload_length = payload_len;
                            parse_state_ = ParseState::READ_MASKING_KEY;
                        } else if (payload_len == 126) {
                            current_frame_.payload_length = 0;
                            parse_state_ = ParseState::READ_PAYLOAD_LENGTH;
                        } else {
                            current_frame_.payload_length = 0;
                            parse_state_ = ParseState::READ_PAYLOAD_LENGTH;
                        }
                        
                        // 移除已处理的头部数据
                        buffer_.erase(buffer_.begin(), buffer_.begin() + 2);
                        bytes_read_ -= 2;
                    }
                }
                break;
            }
            case ParseState::READ_PAYLOAD_LENGTH: {
                size_t needed = (current_frame_.payload_length == 0) ? 2 : 8;
                if (bytes_read_ < needed) {
                    buffer_.insert(buffer_.end(), ptr, end);
                    bytes_read_ += (end - ptr);
                    ptr = end;
                    
                    if (bytes_read_ >= needed) {
                        if (needed == 2) {
                            uint16_t len16 = 0;
                            memcpy(&len16, buffer_.data(), 2);
                            current_frame_.payload_length = ntohs(len16);
                        } else {
                            uint64_t len64 = 0;
                            memcpy(&len64, buffer_.data(), 8);
                            current_frame_.payload_length = be64toh(len64);
                        }
                        
                        buffer_.erase(buffer_.begin(), buffer_.begin() + needed);
                        bytes_read_ -= needed;
                        parse_state_ = ParseState::READ_MASKING_KEY;
                    }
                }
                break;
            }
            case ParseState::READ_MASKING_KEY: {
                if (current_frame_.masked) {
                    if (bytes_read_ < 4) {
                        buffer_.insert(buffer_.end(), ptr, end);
                        bytes_read_ += (end - ptr);
                        ptr = end;
                        
                        if (bytes_read_ >= 4) {
                            memcpy(current_frame_.masking_key, buffer_.data(), 4);
                            buffer_.erase(buffer_.begin(), buffer_.begin() + 4);
                            bytes_read_ -= 4;
                            parse_state_ = ParseState::READ_PAYLOAD;
                        }
                    }
                } else {
                    parse_state_ = ParseState::READ_PAYLOAD;
                }
                break;
            }
            case ParseState::READ_PAYLOAD: {
                size_t needed = current_frame_.payload_length - buffer_.size();
                size_t available = end - ptr;
                size_t to_read = std::min(needed, available);
                
                buffer_.insert(buffer_.end(), ptr, ptr + to_read);
                ptr += to_read;
                
                if (buffer_.size() >= current_frame_.payload_length) {
                    // 完整帧已接收
                    if (current_frame_.masked) {
                        // 解除掩码
                        for (size_t i = 0; i < buffer_.size(); ++i) {
                            buffer_[i] ^= current_frame_.masking_key[i % 4];
                        }
                    }
                    
                    // 处理帧
                    switch (current_frame_.opcode) {
                        case 0x01: // 文本帧
                            PLOG_INFO << "WebSocket Connection received text frame of " << buffer_.size() << " bytes";
                            ProcessTextFrame(buffer_.data(), buffer_.size());
                            break;
                        case 0x02: // 二进制帧
                            PLOG_INFO << "WebSocket Connection received binary frame of " << buffer_.size() << " bytes";
                            ProcessBinaryFrame(buffer_.data(), buffer_.size());
                            break;
                        case 0x08: // 关闭帧
                            PLOG_INFO << "WebSocket Connection received close frame";
                            ProcessCloseFrame(buffer_.data(), buffer_.size());
                            break;
                        case 0x09: // Ping帧
                            PLOG_INFO << "WebSocket Connection received ping frame";
                            ProcessPingFrame(buffer_.data(), buffer_.size());
                            break;
                        case 0x0A: // Pong帧
                            PLOG_INFO << "WebSocket Connection received pong frame";
                            ProcessPongFrame(buffer_.data(), buffer_.size());
                            break;
                    }
                    
                    // 重置状态，准备下一个帧
                    buffer_.clear();
                    bytes_read_ = 0;
                    parse_state_ = ParseState::READ_HEADER;
                    memset(&current_frame_, 0, sizeof(current_frame_));
                }
                break;
            }
        }
    }
}

void WebSocketConnection::ProcessTextFrame(const char* data, size_t len) {
    if (server_ && state_ == State::OPEN) {
        std::shared_ptr<WebSocketConnection> shared_conn(this, [](WebSocketConnection*){});
        
        // 获取协议解析器
        auto protocol = server_->GetServerProtocol();
        
        // 如果没有配置协议解析器，直接调用OnMessage
        if (!protocol) {
            server_->OnMessage(shared_conn, data, len);
            return;
        }
        
        // 使用协议解析器解析数据
        int package_len = 0;
        int msg_len = 0;
        PackageStatus status = protocol->ParsePackage(data, len, package_len, msg_len);
        
        if (status == PackageFull) {
            // 完整包，调用OnMessage
            server_->OnMessage(shared_conn, data, package_len);
        } else if (status == PackageError) {
            // 包错误，关闭连接
            PLOG_ERROR << "WebSocket Connection " << conn_id_ << " package parse error, closing connection";
            Close();
        }
        // PackageLess状态不处理，因为WebSocket是完整帧传输
    }
}

void WebSocketConnection::ProcessBinaryFrame(const char* data, size_t len) {
    if (server_ && state_ == State::OPEN) {
        std::shared_ptr<WebSocketConnection> shared_conn(this, [](WebSocketConnection*){});
        
        // 获取协议解析器
        auto protocol = server_->GetServerProtocol();
        
        // 如果没有配置协议解析器，直接调用OnMessage
        if (!protocol) {
            server_->OnMessage(shared_conn, data, len);
            return;
        }
        
        // 使用协议解析器解析数据
        int package_len = 0;
        int msg_len = 0;
        PackageStatus status = protocol->ParsePackage(data, len, package_len, msg_len);
        
        if (status == PackageFull) {
            // 完整包，调用OnMessage
            server_->OnMessage(shared_conn, data, package_len);
        } else if (status == PackageError) {
            // 包错误，关闭连接
            PLOG_ERROR << "WebSocket Connection " << conn_id_ << " package parse error, closing connection";
            Close();
        }
        // PackageLess状态不处理，因为WebSocket是完整帧传输
    }
}

void WebSocketConnection::ProcessCloseFrame(const char* data, size_t len) {
    state_ = State::CLOSING;
    // 发送关闭响应
    SendFrame(data, len, 0x08);
    Close();
}

void WebSocketConnection::ProcessPingFrame(const char* data, size_t len) {
    // 回复 Pong 帧
    SendFrame(data, len, 0x0A);
}

void WebSocketConnection::ProcessPongFrame(const char* data, size_t len) {
    // 忽略 Pong 帧
}

// WebSocketConnection 其他方法
void WebSocketConnection::Send(const char* data, size_t len) {
    if (state_ != State::OPEN || is_closing_gracefully_) {
        PLOG_INFO << "WebSocket Connection not open or closing, dropping send request";
        return;
    }
    
    // 检查发送队列大小是否超过配置的最大值
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        if (send_queue_.size() >= server_->GetConfig().GetMaxSendQueueSize()) {
            PLOG_WARNING << "WebSocket Connection send queue full, dropping send request";
            return;
        }
    }
    
    PLOG_INFO << "WebSocket Connection sending message of " << len << " bytes";
    SendFrame(data, len, 0x01); // 默认发送文本帧
}

void WebSocketConnection::OnWriteComplete(int status) {
    if (status < 0) {
        if (status != UV_ECANCELED) {
            PLOG_ERROR << "WebSocket Connection write failed: " << uv_strerror(status);
            Close();
        }
        return;
    }
    
    PLOG_INFO << "WebSocket Connection write complete";
    
    // 写成功，重置标志
    is_writing_ = false;
    
    // 尝试发送队列中的下一包数据
    TrySend();
    
    // 检查是否需要优雅关闭
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        if (is_closing_gracefully_ && send_queue_.empty() && !is_writing_) {
            PLOG_INFO << "WebSocket Connection send queue empty, closing gracefully";
            // 发送队列已空，执行实际关闭
            uv_close((uv_handle_t*)&handle_, [](uv_handle_t* handle) {
                WebSocketConnection* conn = static_cast<WebSocketConnection*>(handle->data);
                conn->state_ = State::CLOSED;
                // 计算在线时长（秒）
                size_t now = uv_now(uv_default_loop());
                double online_seconds = (now - conn->create_time_) / 1000.0;
                PLOG_INFO << "WebSocket Connection " << conn->conn_id_ << " closed gracefully, online time: " << online_seconds << " seconds";
                // 触发用户层的 OnClose
                if (conn->server_) {
                    std::shared_ptr<WebSocketConnection> shared_conn(conn, [](WebSocketConnection*){});
                    conn->server_->OnClose(shared_conn);
                }
                delete conn; // 最终释放 Connection 对象
            });
        }
    }
}

void WebSocketConnection::Close() {
    if (state_ == State::CLOSED || state_ == State::CLOSING || is_closing_gracefully_) {
        return;
    }
    
    // 停止心跳
    StopHeartbeat();
    
    // 检查发送队列是否为空
    bool is_empty = false;
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        is_empty = send_queue_.empty();
    }
    
    if (is_empty && !is_writing_) {
        // 发送队列为空，直接关闭
        state_ = State::CLOSING;
        PLOG_INFO << "WebSocket Connection closing immediately";
        
        // 关闭 handle，触发 close 回调
        uv_close((uv_handle_t*)&handle_, [](uv_handle_t* handle) {
            WebSocketConnection* conn = static_cast<WebSocketConnection*>(handle->data);
            conn->state_ = State::CLOSED;
            // 计算在线时长（秒）
            size_t now = uv_now(uv_default_loop());
            double online_seconds = (now - conn->create_time_) / 1000.0;
            PLOG_INFO << "WebSocket Connection " << conn->conn_id_ << " closed immediately, online time: " << online_seconds << " seconds";
            // 触发用户层的 OnClose
            if (conn->server_) {
                std::shared_ptr<WebSocketConnection> shared_conn(conn, [](WebSocketConnection*){});
                conn->server_->OnClose(shared_conn);
            }
            delete conn; // 最终释放 Connection 对象
        });
    } else {
        // 发送队列不为空，执行优雅关闭
        is_closing_gracefully_ = true;
        state_ = State::CLOSING;
        PLOG_INFO << "WebSocket Connection closing gracefully";
        // 不立即关闭，等待发送队列处理完毕
    }
}

void WebSocketConnection::StartHeartbeat() {
    if (is_heartbeat_running_) {
        return;
    }
    
    is_heartbeat_running_ = true;
    last_active_time_ = uv_now(uv_default_loop());
    
    // 启动心跳定时器，间隔为配置的心跳间隔
    uv_timer_start(&heartbeat_timer_, [](uv_timer_t* timer) {
        WebSocketConnection* conn = static_cast<WebSocketConnection*>(timer->data);
        conn->OnHeartbeatTimeout();
    }, server_->GetConfig().GetHeartbeatInterval(), server_->GetConfig().GetHeartbeatInterval());
    
    PLOG_INFO << "WebSocket Connection heartbeat started, interval: " << server_->GetConfig().GetHeartbeatInterval() << "ms";
}

void WebSocketConnection::StopHeartbeat() {
    if (!is_heartbeat_running_) {
        return;
    }
    
    uv_timer_stop(&heartbeat_timer_);
    is_heartbeat_running_ = false;
    PLOG_INFO << "WebSocket Connection heartbeat stopped";
}

void WebSocketConnection::OnHeartbeatTimeout() {
    size_t now = uv_now(uv_default_loop());
    size_t interval = server_->GetConfig().GetHeartbeatInterval();
    
    // 检查上次活跃时间是否超过心跳间隔的两倍
    if (now - last_active_time_ > interval * 2) {
        PLOG_WARNING << "WebSocket Connection heartbeat timeout, closing connection";
        Close();
    }
    // 否则，继续等待下一次心跳检查
}

void WebSocketConnection::OnDataReceived(const char* data, size_t len) {
    PLOG_INFO << "WebSocket Connection received data of " << len << " bytes, state: " << static_cast<int>(state_);
    
    if (state_ == State::HANDSHAKE) {
        // 处理握手数据
        handshake_buffer_.insert(handshake_buffer_.end(), data, data + len);
        
        // 检查是否接收到完整的握手请求
        if (std::search(handshake_buffer_.begin(), handshake_buffer_.end(), "\r\n\r\n", "\r\n\r\n" + 4) != handshake_buffer_.end()) {
            // 将 vector<char> 转换为 string 传递给 ParseHandshake
            std::string handshake_str(handshake_buffer_.begin(), handshake_buffer_.end());
            ParseHandshake(handshake_str);
        }
    } else {
        // 处理 WebSocket 帧
        ParseFrame(data, len);
    }
}

} // namespace uv_net