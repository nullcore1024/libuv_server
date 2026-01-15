#ifndef FIX_SIZE_PROTOCOL_H
#define FIX_SIZE_PROTOCOL_H

#include "server_protocol.h"
#include <arpa/inet.h> // 用于网络字节序转换

// FixSizeProtocol类实现了基于4字节网络字节序size定界的协议
class FixSizeProtocol : public ServerProtocol {
public:
    virtual ~FixSizeProtocol() = default;
    
    // 解析包
    // buff: 缓冲区指针
    // len: 缓冲区长度
    // package_len: 输出参数，返回完整包的长度
    // msg_len: 输出参数，返回消息内容的长度
    // 返回值: PackageStatus，表示解析结果
    PackageStatus ParsePackage(const char* buff, size_t len, int& package_len, int& msg_len) override {
        // 检查缓冲区长度是否至少为4个字节（size字段的长度）
        if (len < 4) {
            return PackageLess; // 包不完整，需要更多数据
        }
        
        // 读取4字节的网络字节序size，并转换为主机字节序
        uint32_t net_size;
        memcpy(&net_size, buff, 4);
        int total_len = ntohl(net_size);
        
        // 检查包大小是否合理（至少包含size字段，且不超过最大限制）
        if (total_len < 4 || total_len > 65535) {
            return PackageError; // 包大小不合理
        }
        
        // 检查缓冲区长度是否大于等于完整包的长度
        if (len < static_cast<size_t>(total_len)) {
            return PackageLess; // 包不完整，需要更多数据
        }
        
        // 设置输出参数
        package_len = total_len; // 完整包的长度（包含size字段）
        msg_len = total_len - 4; // 消息内容的长度（不包含size字段）
        
        return PackageFull; // 包完整
    }
};

#endif // FIX_SIZE_PROTOCOL_H