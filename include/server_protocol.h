#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#include <cstddef>

enum PackageStatus: int {
	// PackageLess shows is not a completed package.
	PackageLess = 0,
	// PackageFull shows is a completed package.
	PackageFull = 1,
	// PackageError shows is a error package.
	PackageError = 2,
};

class ServerProtocol {
public:
    virtual ~ServerProtocol() = default;
    // 解析包
    virtual PackageStatus ParsePackage(const char* buff, size_t len, int& package_len, int& msg_len) = 0;
};

#endif // SERVER_PROTOCOL_H