#ifndef UV_NET_WEBSOCKET_CONNECTION_H
#define UV_NET_WEBSOCKET_CONNECTION_H

#include "connection.h"
#include "tcp_connection.h"
#include <queue>
#include <mutex>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace uv_net {

class WebSocketServer;

// WebSocket 连接实现
class WebSocketConnection : public TcpConnection {
public:
    WebSocketConnection(WebSocketServer* server);
    ~WebSocketConnection() override;

    // 业务调用的 Send
    void Send(const char* data, size_t len) override;
    void Close() override;

    // 内部逻辑
    void OnWriteComplete(int status) override;
    void OnHandshakeComplete();
    void ParseFrame(const char* data, size_t len);
    bool ParseHandshake(const std::string& handshake_data);
    void ProcessTextFrame(const char* data, size_t len);
    void ProcessBinaryFrame(const char* data, size_t len);
    void ProcessCloseFrame(const char* data, size_t len);
    void ProcessPingFrame(const char* data, size_t len);
    void ProcessPongFrame(const char* data, size_t len);
    void OnDataReceived(const char* data, size_t len) override; // 重写父类的方法，处理WebSocket数据
    
    // 重写父类的方法
    void TrySend() override;
    void StartHeartbeat() override;
    void StopHeartbeat() override;
    void OnHeartbeatTimeout() override;

    // WebSocket 相关状态
    enum class State {
        HANDSHAKE,
        OPEN,
        CLOSING,
        CLOSED
    };

    State state_;
    std::vector<char> handshake_buffer_;

private:
    // WebSocket 帧结构
    struct WebSocketFrame {
        bool fin;          // 是否为最终帧
        uint8_t opcode;    // 操作码
        bool masked;       // 是否有掩码
        uint64_t payload_length; // 有效负载长度
        uint8_t masking_key[4];  // 掩码键
        const char* payload;      // 有效负载数据
    };

    // 帧解析状态
    enum class ParseState {
        READ_HEADER,
        READ_PAYLOAD_LENGTH,
        READ_MASKING_KEY,
        READ_PAYLOAD
    };

    ParseState parse_state_;
    WebSocketFrame current_frame_;
    size_t bytes_read_;
    std::vector<char> buffer_;

    // 辅助方法
    std::string GenerateResponseKey(const std::string& sec_websocket_key);
    void SendHandshakeResponse(const std::string& sec_websocket_key);
    void SendFrame(const char* data, size_t len, uint8_t opcode);
};

} // namespace uv_net

#endif