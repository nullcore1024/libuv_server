cmake_minimum_required(VERSION 3.10)
project(UvNetLib)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用pkg-config获取依赖库信息
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED libuv)
pkg_check_modules(OPENSSL REQUIRED openssl)

# 打印调试信息
message(STATUS "libuv include dirs: ${LIBUV_INCLUDE_DIRS}")
message(STATUS "libuv libraries: ${LIBUV_LIBRARIES}")
message(STATUS "libuv version: ${LIBUV_VERSION}")
message(STATUS "OpenSSL include dirs: ${OPENSSL_INCLUDE_DIRS}")
message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")

# 头文件路径
include_directories(${LIBUV_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)

# 库源码
set(LIB_SRCS
    src/uv_net/tcp_connection.cpp
    src/uv_net/tcp_server.cpp
    src/uv_net/udp_connection.cpp
    src/uv_net/udp_server.cpp
    src/uv_net/websocket_connection.cpp
    src/uv_net/websocket_server.cpp
    src/uv_net/utils.cpp
)

# 生成静态库
add_library(uv_net STATIC ${LIB_SRCS})
target_link_libraries(uv_net ${LIBUV_LIBRARIES} ${OPENSSL_LIBRARIES})

# 生成可执行文件
add_executable(echo_server example/echo_server.cpp)
target_link_libraries(echo_server uv_net ${LIBUV_LIBRARIES} ${OPENSSL_LIBRARIES})

# 生成 WebSocket 示例服务器
add_executable(ws_echo_server example/ws_echo_server.cpp)
target_link_libraries(ws_echo_server uv_net ${LIBUV_LIBRARIES} ${OPENSSL_LIBRARIES})
