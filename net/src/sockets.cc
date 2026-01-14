#include "sockets.h"
#include "Logger.h"
#include "InetAddress.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace sockets;

// 私有工具函数：错误日志打印
namespace
{
constexpr int kInvalidFd = -1;

void setSocketIntOpt(int sockfd, int level, int optname, int optval)
{
    if (::setsockopt(sockfd, level, optname, &optval, sizeof(optval)) < 0)
    {
        LOG_ERROR("sockets::setSocketIntOpt fail, sockfd=%d, optname=%d, errno=%d, info=%s",
                  sockfd, optname, errno, strerror(errno));
    }
}
}

int sockets::createNonblockingOrDie(sa_family_t family)
{
    // 纯IPv4: SOCK_STREAM | 非阻塞 | CLOEXEC，协议TCP
    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("sockets::createNonblockingOrDie fail, errno=%d, info=%s", errno, strerror(errno));
        abort();
    }
    return sockfd;
}

int sockets::connect(int sockfd, const struct sockaddr* addr)
{
    // IPv4 固定地址长度 sizeof(struct sockaddr_in)
    return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in)));
}

void sockets::bindOrDie(int sockfd, const struct sockaddr* addr)
{
    int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in)));
    if (ret < 0)
    {
        LOG_FATAL("sockets::bindOrDie fail, sockfd=%d, errno=%d, info=%s", sockfd, errno, strerror(errno));
        abort();
    }
}

void sockets::listenOrDie(int sockfd)
{
    int ret = ::listen(sockfd, SOMAXCONN);
    if (ret < 0)
    {
        LOG_FATAL("sockets::listenOrDie fail, sockfd=%d, errno=%d, info=%s", sockfd, errno, strerror(errno));
        abort();
    }
}

int sockets::accept(int sockfd, struct sockaddr_in* peeraddr)
{
    // IPv4 地址长度
    socklen_t addrlen = sizeof(*peeraddr);
    // accept4 创建 非阻塞 + CLOEXEC 的fd，纯IPv4
    int connfd = ::accept4(sockfd, sockaddr_cast(peeraddr), &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd < 0)
    {
        int savedErrno = errno;
        LOG_ERROR("sockets::accept fail, sockfd=%d, errno=%d, info=%s", sockfd, savedErrno, strerror(savedErrno));
        switch (savedErrno)
        {
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO:
            case EPERM:
            case EMFILE:
                break;
            default:
                LOG_ERROR("sockets::accept unexpected error, sockfd=%d, errno=%d, info=%s", sockfd, savedErrno, strerror(savedErrno));
                break;
        }
    }
    return connfd;
}

// ✅ 你的核心函数：带日志校验的安全close，彻底解决fd泄漏
void sockets::close(int sockfd)
{
    if (sockfd == kInvalidFd)
    {
        return;
    }
    if (::close(sockfd) < 0)
    {
        LOG_ERROR("sockets::close fail, sockfd=%d, errno=%d, info=%s", sockfd, errno, strerror(errno));
    }
}

void sockets::setNonblocking(int sockfd)
{
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    if (ret < 0)
    {
        LOG_ERROR("sockets::setNonblocking fail, sockfd=%d, errno=%d, info=%s", sockfd, errno, strerror(errno));
    }
}

void sockets::setCloseOnExec(int sockfd)
{
    int flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    int ret = ::fcntl(sockfd, F_SETFD, flags);
    if (ret < 0)
    {
        LOG_ERROR("sockets::setCloseOnExec fail, sockfd=%d, errno=%d, info=%s", sockfd, errno, strerror(errno));
    }
}

void sockets::setTcpNoDelay(int sockfd)
{
    setSocketIntOpt(sockfd, IPPROTO_TCP, TCP_NODELAY, 1);
}

void sockets::setReuseAddr(int sockfd)
{
    setSocketIntOpt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1);
}

void sockets::setReusePort(int sockfd)
{
    setSocketIntOpt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1);
}

void sockets::setKeepAlive(int sockfd)
{
    setSocketIntOpt(sockfd, SOL_SOCKET, SO_KEEPALIVE, 1);
}

// ✅ IPv4专属强转：sockaddr_in -> sockaddr
const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in* addr)
{
    return reinterpret_cast<const struct sockaddr*>(addr);
}

struct sockaddr* sockets::sockaddr_cast(struct sockaddr_in* addr)
{
    return reinterpret_cast<struct sockaddr*>(addr);
}

// ✅ 修复BUG：统一调用 InetAddress 的 setSockAddr (IPv4)
void sockets::getLocalAddr(int sockfd, InetAddress* localAddr)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t addrlen = sizeof(addr);
    if (::getsockname(sockfd, sockaddr_cast(&addr), &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr fail, sockfd=%d, errno=%d, info=%s", sockfd, errno, strerror(errno));
    }
    localAddr->setSockAddr(addr);
}

// ✅ 修复BUG：统一调用 InetAddress 的 setSockAddr (IPv4)
void sockets::getPeerAddr(int sockfd, InetAddress* peerAddr)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t addrlen = sizeof(addr);
    if (::getpeername(sockfd, sockaddr_cast(&addr), &addrlen) < 0)
    {
        LOG_ERROR("sockets::getPeerAddr fail, sockfd=%d, errno=%d, info=%s", sockfd, errno, strerror(errno));
    }
    peerAddr->setSockAddr(addr);
}

bool sockets::isSelfConnect(int sockfd)
{
    InetAddress localAddr, peerAddr;
    getLocalAddr(sockfd, &localAddr);
    getPeerAddr(sockfd, &peerAddr);
    return localAddr.toIpPort() == peerAddr.toIpPort();
}

int sockets::getSocketError(int sockfd)
{
    int optval;
    socklen_t optlen = sizeof(optval);
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        return errno;
    }
    return optval;
}