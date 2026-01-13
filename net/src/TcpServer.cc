#include "TcpServer.h"

#include <functional>
#include <strings.h>


static EventLoop* CheckLoopNotNull(EventLoop *loop) 
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}
 
/**
 *  这里给的loop就是用户创建的mainLoop上面运行的acceptor监听新连接
 * 
 *  acceptor首先根据监听地址创建非阻塞的sockfd，然后封装进acceptChannel，
 *  将这个channel添加到运行在mainloop上的poller进行监听（关注读事件）
 */
TcpServer::TcpServer(EventLoop *loop, 
            const InetAddress &listenAddr, 
            const std::string &nameArg,
            Option option)
            : loop_(CheckLoopNotNull(loop))
            , ipPort_(listenAddr.toIpPort())
            , name_(nameArg)
            , acceptor_(new Acceptor(loop, listenAddr, option == KReusePost))
            , threadPool_(new EventLoopThreadPool(loop, name_))
            , connectionCallback_()
            , messageCallback_()
            , nextConnId_(1)
            , started_(0)
{   
    // 当有新用户连接的时候会执行这个回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {   
        // 这是一个栈上的对象，TcpConnectionPtr是shared_ptr智能指针
        // 如果直接reset会导致引用计数变为0，TcpConnection对象被销毁
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{   
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{   
    if (started_++ == 0)   // 防止一个TcpSercver对象被start多次
    {
        // 启动底层的loop线程池
        threadPool_->start(threadInitCallback_);      
        // 开始监听 acceptChannel有无感兴趣的新事件（新连接）
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新的用户连接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 轮询算法选择一个subloop
    EventLoop *ioLoop = threadPool_->getNextLoop();
    // 生成一个TcpConnection连接对象
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection from %s \n",
        connName.c_str(), peerAddr.toIpPort().c_str());

    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("TcpServer::newConnection - getsockname error \n");
    }
    InetAddress localAddr(local);

    // 创建TcpConnection对象
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,
        connName,
        sockfd,
        localAddr,
        peerAddr));

    connections_[connName] = conn;
    
    // 下面的回调都是用户设置给TcpServer的，然后TcpServer传递给TcpConnection
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnction, this, std::placeholders::_1));

    // 直接调用TcpConnection的connectEstablished方法，表示连接建立成功
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnction(const TcpConnectionPtr &conn)
{
    // 该函数可能在非loop线程中调用
    loop_->runInLoop(std::bind(&TcpServer::removeConnctionInLoop, this, conn));
}

void TcpServer::removeConnctionInLoop(const TcpConnectionPtr & conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection \n", conn->name().c_str());
    
    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    // 在ioLoop中销毁连接
    ioLoop->queueLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
