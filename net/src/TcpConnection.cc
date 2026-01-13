#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Buffer.h"

#include <functional>
#include <errno.h>
#include <unistd.h> 

static EventLoop* CheckLoopNotNull(EventLoop *loop) 
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
        const std::string &nameArg,
        int sockfd,
        const InetAddress& localAddr,
        const InetAddress& peerAddr)
        : loop_(CheckLoopNotNull(loop))
        , name_(nameArg)
        , state_(kConnecting)
        , reading_(true)
        , socket_(new Socket(sockfd))
        , channel_(new Channel(loop, sockfd))
        , localAddr_(localAddr)
        , peerAddr_(peerAddr)
        , highWaterMark_(64*1024*1024)  // 64M
{   
    channel_->setReadCallback(
    std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
    std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
    std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
    std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", 
        name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    // 每个连接对应一个socked和Channel
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if (n > 0)
    {
        // 已经建立的用户，有可读事件发生，调用用户传入的回调函数onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{   
    if (channel_->isWriting())
    {      
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {   
                    // 唤醒loop_对一个的thread线程
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
            }
            if (state_ == kDisconnecting)
            {
                shutdownInLoop();
            }
        }
        else 
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
    
}

// 关闭连接的回调，poller发现tcp连接的读事件发生了，但是读到的数据为0，说明对端关闭了连接
// Channel::closeCallback_ => TcpConnection::handleClose => TcpServer::removeConnectionInLoop
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);       // 执行连接关闭的回调
    closeCallback_(connPtr);            // 执行关闭连接的回调
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d", name_.c_str(), err);
}

void TcpConnection::send(const std::string &buf)
{   
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLooop(buf.c_str(), buf.size());
        }
        else
        {   
            loop_->runInLoop(std::bind(&TcpConnection::sendInLooop, this, buf.c_str(), buf.size()));
        }
    }
}

void TcpConnection::shutdown()
{ 
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// 发送数据，应用写的快，内核发送数据慢，需要把待发送的数据写入缓冲过去，而且设置了水位回调
void TcpConnection::sendInLooop(const void* message, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected)
    {
        LOG_ERROR("TcpConnection::sendInLoop disconnected, give up writing \n");
        return;
    }

    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), message, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            // 一次性发送完成
            if (remaining == 0 && writeCompleteCallback_)
            {   
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else 
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }   
    // 说明当前这一次write，并没有发数据发送完成，剩余的数据需要保存在缓冲区中，然后给channel
    // 注册epollout事件，poller监听到tcp连接的发送缓冲区可写后，再发送剩余的数据
    // 也就是调用TCP Connection::handleWrite函数发送剩余的数据
    if (!faultError && remaining > 0)
    {   
        // 目前发送缓冲区中的数据长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((const char*)message + nwrote, remaining);
        if (!channel_->isWriting())
        {   
            // 这里必须要注册channel的写事件，否则poller不会监听tcp连接的发送缓冲区是否可写
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 说明发送缓冲区的数据已经发送完成
    {
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向poller注册channel的读事件

    // 已经建立连接，执行用户传入的回调操作
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}