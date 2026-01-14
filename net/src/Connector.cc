#include "Connector.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Socket.h"
#include "Logger.h"
#include "sockets.h"

#include <errno.h>
#include <unistd.h>
#include <functional>
#include <strings.h>
#include <thread>

const int Connector::kInitRetryDelayMs;
const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, 
    const InetAddress &serverAddr)
    : loop_(loop)
    , serverAddr_(serverAddr)
    , connect_(false)
    , state_(kDisconnected)
    , retryDelayMs_(kInitRetryDelayMs)
{
    LOG_INFO("Connector created");
}

Connector::~Connector()
{
    LOG_INFO("Connector destroyed");
    stop();
}

void Connector::start()
{
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));     
}

void Connector::startInLoop()
{   

    if (loop_->isInLoopThread()) {
        if (connect_)
        {
            connect();
        }
        else 
        {
            LOG_INFO("Connector is stopped");
        }
    } else {
        LOG_FATAL("Connector::startInLoop - not in loop thread");
    }
}

void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
}

void Connector::stopInLoop()
{   
    if (loop_->isInLoopThread()) {
        if (state_ == kConnecting)
        {
            setState(kDisconnected);
            int sockfd = removeAndResetChannel();
            retry(sockfd); // 停止连接，清理资源
        }
    } else {
        LOG_FATAL("Connector::stopInLoop - not in loop thread");
    }
}

void Connector::restart()
{
    if (loop_->isInLoopThread()) 
    {
        setState(kDisconnected);
        retryDelayMs_ = kInitRetryDelayMs;
        connect_ = true;
        start();
    }
    else 
    {
        LOG_FATAL("Connector::restart - not in loop thread");
    }
}

void Connector::connect()
{   
    int sockfd = sockets::createNonblockingOrDie();
    int ret = sockets::connect(sockfd, sockets::sockaddr_cast(serverAddr_.getSockAddr()));
    int saveErrno = (ret == 0) ? 0 : errno;
    switch (saveErrno)
    {
        case 0: // 连接成功
        case EINPROGRESS: // 非阻塞连接，正在进行中（正常）
        case EINTR:       // 被信号中断，重试
        case EISCONN:     // 已经连接成功
            connecting(sockfd);
            break;

        case EAGAIN:      // 端口暂时不可用，重试
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockfd); // 连接失败，重试
            break;

        case EACCES:
        case EPERM:
        case EFAULT:
        case ENOTSOCK:
            LOG_ERROR("connect error in Connector::connect: errno = %d, sockfd = %d", 
              saveErrno, sockfd);
            sockets::close(sockfd);
            break;

        default:
            LOG_ERROR("Unexpected error in Connector::connect: errno = %d, sockfd = %d", 
              saveErrno, sockfd);
            sockets::close(sockfd);
            break;
    }
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    channel_ = std::unique_ptr<Channel>(new Channel(loop_, sockfd));
    channel_->setWriteCallback(std::bind(&Connector::handleWrite, this));
    channel_->setErrorCallback(std::bind(&Connector::handleError, this));
    channel_->enableWriting(); // 监听可写事件
}

int Connector::removeAndResetChannel()
{   
    // 注销所有事件，对所有事件不感兴趣
    channel_->disableAll();
    // 从Loop中移除Channel
    channel_->remove();
    int sockfd = channel_->fd();
    loop_->queueInLoop(std::bind(&Connector::resetChannel, this));
    return sockfd;
}

void Connector::resetChannel()
{   
    // 这里是指针的方法，所以不是->
    channel_.reset();
}

void Connector::handleWrite()
{
    LOG_INFO("Connector::handleWrite");
    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err)
        {
            LOG_ERROR("Connector::handleWrite - SO_ERROR = %d", err);
            retry(sockfd);
        }
        else if (sockets::isSelfConnect(sockfd))
        {
            LOG_ERROR("Connector::handleWrite - self connect");
            retry(sockfd);
        }
        else  // 连接成功
        {
            setState(kConnected);
            if (connect_)
            {
                newConnectCallback_(sockfd);    // 回调给TcpClient
            }
            else 
            {
                sockets::close(sockfd);
            }
        } 
    }
}

void Connector::handleError()
{
    LOG_ERROR("Connector::handleError");
    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        LOG_INFO("Connector::handleError - SO_ERROR = %d", err);
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    sockets::close(sockfd);
    setState(kDisconnected);
    if (connect_)
    {
        LOG_INFO("Connector::retry - Retry connecting to %s in %d ms",
                 serverAddr_.toIpPort().c_str(), retryDelayMs_);

        int delayMs = this->retryDelayMs_;
        this->retryDelayMs_ = std::min(this->retryDelayMs_ * 2, Connector::kMaxRetryDelayMs);
        std::thread([this, delayMs]() {
            // 睡眠指定毫秒数，不阻塞IO线程的EventLoop
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            // 睡醒后，投递任务到IO线程执行，保证所有操作在单一线程
            loop_->runInLoop(std::bind(&Connector::startInLoop, this));
        }).detach(); // 线程执行完毕自动销毁，无内存泄漏，无join阻塞
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
    else
    {
        LOG_INFO("Connector::retry - stopped");
    }
}