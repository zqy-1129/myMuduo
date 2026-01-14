#pragma once

#include "noncopyable.h"
#include "InetAddress.h"

#include <sys/socket.h>
#include <unistd.h>

// 全局的socket系统调用封装，纯IPv4版本，线程安全，无状态
namespace sockets
{
// 创建 非阻塞 + CLOEXEC 模式的TCP socket fd，失败直接终止程序
int createNonblockingOrDie(sa_family_t family = AF_INET);

// 原生connect封装，非阻塞/阻塞都可用
int connect(int sockfd, const struct sockaddr* addr);

// 原生bind封装
void bindOrDie(int sockfd, const struct sockaddr* addr);

// 原生listen封装
void listenOrDie(int sockfd);

// 原生accept封装，返回新连接的fd，同时通过peeraddr传出对端IPv4地址
int accept(int sockfd, struct sockaddr_in* peeraddr);

// 带日志校验的安全close，彻底解决fd泄漏问题，核心函数
void close(int sockfd);

// 设置fd为非阻塞模式
void setNonblocking(int sockfd);

// 设置fd的CLOEXEC标志 (fork子进程时自动关闭fd，防泄漏)
void setCloseOnExec(int sockfd);

// 设置TCP_NODELAY，禁用Nagle算法，小包实时发送（TCP必备）
void setTcpNoDelay(int sockfd);

// 设置SO_REUSEADDR，端口重启快速复用（服务端必备）
void setReuseAddr(int sockfd);

// 设置SO_REUSEPORT，多进程监听同一个端口（高并发服务端用）
void setReusePort(int sockfd);

// 设置SO_KEEPALIVE，TCP心跳保活
void setKeepAlive(int sockfd);

// IPv4专属 地址类型强转工具函数
const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
struct sockaddr* sockaddr_cast(struct sockaddr_in* addr);

// 从fd中获取本地IPv4地址，写入InetAddress
void getLocalAddr(int sockfd, InetAddress* localAddr);

// 从fd中获取对端IPv4地址，写入InetAddress
void getPeerAddr(int sockfd, InetAddress* peerAddr);

// 检查socket是否自连接
bool isSelfConnect(int sockfd);

// 获取socket的错误码 (核心：非阻塞connect后判断连接是否成功)
int getSocketError(int sockfd);

} 