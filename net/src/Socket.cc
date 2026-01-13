#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

/**
 * 这里就是封装创建套接字的，与之前select、poll、epoll
 * 中创建套接字的流程一致，创建->绑定->监听
 */

Socket::~Socket()
{
    close(sockfd_);
}

// 绑定本地地址和sockfd
void Socket::bindAddress(const InetAddress &localaddr)
{    
    if (::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in)) != 0)
    {
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
    }
}

// 监听
void Socket::listen()
{
    if (::listen(sockfd_, 1024) != 0) 
    {
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_)
    }
}

// 新的连接请求
int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    // 这里len必须初始化，否则accept会失败
    socklen_t len = sizeof addr;
    bzero(&addr, sizeof addr);
    // 这里使用accept4，一次性设置非阻塞和close on exec属性
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0) 
    {
        LOG_ERROR("shutdonwWrite error");
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on)
{
int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}