#ifndef UV_NET_SERVER_CONFIG_H
#define UV_NET_SERVER_CONFIG_H

#include <cstddef>
#include <cstdint>

namespace uv_net {

// 服务器配置类
class ServerConfig {
public:
    ServerConfig() : 
        read_buffer_size_(8192),          // 默认8KB读缓冲区
        write_buffer_size_(8192),         // 默认8KB写缓冲区
        max_connections_(10000),          // 默认最大连接数10000
        max_send_queue_size_(1000),       // 默认最大发送队列大小
        max_package_size_(65536),         // 默认最大包大小64KB
        connection_read_timeout_(30000),  // 默认连接读超时30秒
        heartbeat_interval_(60000),       // 默认心跳间隔60秒
        tcp_no_delay_(true)               // 默认启用TCP_NODELAY
    {}

    // 读缓冲区大小设置
    void SetReadBufferSize(size_t size) { read_buffer_size_ = size; }
    size_t GetReadBufferSize() const { return read_buffer_size_; }

    // 写缓冲区大小设置
    void SetWriteBufferSize(size_t size) { write_buffer_size_ = size; }
    size_t GetWriteBufferSize() const { return write_buffer_size_; }

    // 最大连接数设置
    void SetMaxConnections(size_t max) { max_connections_ = max; }
    size_t GetMaxConnections() const { return max_connections_; }

    // 最大发送队列大小设置
    void SetMaxSendQueueSize(size_t size) { max_send_queue_size_ = size; }
    size_t GetMaxSendQueueSize() const { return max_send_queue_size_; }

    // 连接读超时设置（毫秒）
    void SetConnectionReadTimeout(int64_t timeout_ms) { connection_read_timeout_ = timeout_ms; }
    int64_t GetConnectionReadTimeout() const { return connection_read_timeout_; }

    // 心跳间隔设置（毫秒）
    void SetHeartbeatInterval(int64_t interval_ms) { heartbeat_interval_ = interval_ms; }
    int64_t GetHeartbeatInterval() const { return heartbeat_interval_; }

    // 最大包大小设置
    void SetMaxPackageSize(size_t size) { max_package_size_ = size; }
    size_t GetMaxPackageSize() const { return max_package_size_; }

    // TCP_NODELAY设置
    void SetTcpNoDelay(bool enable) { tcp_no_delay_ = enable; }
    bool GetTcpNoDelay() const { return tcp_no_delay_; }

private:
    size_t read_buffer_size_;          // 读缓冲区大小
    size_t write_buffer_size_;         // 写缓冲区大小
    size_t max_connections_;           // 最大连接数
    size_t max_send_queue_size_;       // 最大发送队列大小
    size_t max_package_size_;          // 最大包大小
    int64_t connection_read_timeout_;   // 连接读超时（毫秒）
    int64_t heartbeat_interval_;        // 心跳间隔（毫秒）
    bool tcp_no_delay_;                // TCP_NODELAY开关
};

} // namespace uv_net

#endif