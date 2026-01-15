#ifndef UV_NET_WEBSOCKET_CONNECTION_H
#define UV_NET_WEBSOCKET_CONNECTION_H

#include "connection.h"
#include <queue>
#include <mutex>
#include <vector>
#include <cstdint>

namespace uv_net {

class WebSocketServer;

// WebSocket 连接实现
class WebSocketConnection : public Connection {
public:
    WebSocketConnection(WebSocketServer* server);
    ~WebSocketConnection() override;

    // 业务调用的 Send
    void Send(const char* data, size_t len) override;
    void Close() override;
    std::string GetIP() override;
    int GetPort() override;
    uint32_t GetConnId() override;

    // 内部逻辑
    void OnWriteComplete(int status);
    void OnHandshakeComplete();
    void TrySend();
    void ParseFrame(const char* data, size_t len);
    bool ParseHandshake(const std::string& handshake_data);
    void ProcessTextFrame(const char* data, size_t len);
    void ProcessBinaryFrame(const char* data, size_t len);
    void ProcessCloseFrame(const char* data, size_t len);
    void ProcessPingFrame(const char* data, size_t len);
    void ProcessPongFrame(const char* data, size_t len);

    // WebSocket 相关状态
    enum class State {
        HANDSHAKE,
        OPEN,
        CLOSING,
        CLOSED
    };

    uv_tcp_t handle_;
    WebSocketServer* server_;
    std::string ip_;
    int port_;
    uint32_t conn_id_; // 连接ID
    State state_;
    std::string handshake_buffer_;

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

    // 写请求结构体，携带数据缓冲区
    struct WriteReq {
        uv_write_t req;
        std::string data; // 持有数据，防止在回调结束前被释放
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

    std::queue<std::string> send_queue_;
    bool is_writing_;
    std::mutex send_mutex_;
    
    // 优雅关闭相关
    bool is_closing_gracefully_;
    
    // 心跳相关
    uv_timer_t heartbeat_timer_;
    int64_t last_active_time_;
    int64_t create_time_; // 创建时间（毫秒）
    bool is_heartbeat_running_;
    
    // 内部方法
    void StartHeartbeat();
    void StopHeartbeat();
    void OnHeartbeatTimeout();

    // 辅助方法
    std::string GenerateResponseKey(const std::string& sec_websocket_key);
    void SendHandshakeResponse(const std::string& sec_websocket_key);
    void SendFrame(const char* data, size_t len, uint8_t opcode);
};

} // namespace uv_net

#endif