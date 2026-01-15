#ifndef UV_NET_BUFFER_POOL_H
#define UV_NET_BUFFER_POOL_H

#include <queue>
#include <mutex>
#include <cstddef>

namespace uv_net {

// 缓冲区池，用于管理内存缓冲区，避免频繁的内存分配和释放
class BufferPool {
public:
    explicit BufferPool(size_t buffer_size)
        : buffer_size_(buffer_size) {
    }

    ~BufferPool() {
        // 释放所有缓冲区
        while (!free_buffers_.empty()) {
            delete[] free_buffers_.front();
            free_buffers_.pop();
        }
    }

    // 获取一个缓冲区
    char* AcquireBuffer() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_buffers_.empty()) {
            // 如果没有空闲缓冲区，创建一个新的
            return new char[buffer_size_];
        }
        char* buffer = free_buffers_.front();
        free_buffers_.pop();
        return buffer;
    }

    // 归还一个缓冲区
    void ReleaseBuffer(char* buffer) {
        std::lock_guard<std::mutex> lock(mutex_);
        free_buffers_.push(buffer);
    }

    // 获取缓冲区大小
    size_t GetBufferSize() const {
        return buffer_size_;
    }

private:
    size_t buffer_size_; // 每个缓冲区的大小
    std::queue<char*> free_buffers_; // 空闲缓冲区队列
    std::mutex mutex_; // 保护队列的互斥锁
};

} // namespace uv_net

#endif // UV_NET_BUFFER_POOL_H
