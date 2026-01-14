#include <functional>
#include <atomic>
#include <cstdio>
#include <string>

#include "TcpClient.h"

// 静态全局变量：生成唯一的连接名称
static std::atomic<int> s_connId(0);
static std::string getConnectionName(const std::string& clientName)
{
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "%s#%d", clientName.c_str(), ++s_connId);
    return buf;
}

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr, const std::string& name)
    : loop_(loop)
    , connector_(new Connector(loop, serverAddr))
    , name_(name)
    , retry_(false)
    , connect_(false)
    , state_(kDisconnected)
    , highWaterMark_(64*1024*1024) // 默认高水位：64MB
{
    // 核心绑定：Connector连接成功后，回调TcpClient的newConnection函数，传入可用的sockfd
    connector_->setNewConnectionCallback(
        std::bind(&TcpClient::newConnection, this, std::placeholders::_1)
    );
    LOG_INFO("TcpClient::TcpClient [%s] - create client, connect to %s", 
             name_.c_str(), serverAddr.toIpPort().c_str());
}

TcpClient::~TcpClient()
{
    LOG_INFO("TcpClient::~TcpClient [%s]", name_.c_str());
    std::shared_ptr<TcpConnection> conn;
    conn = connection_; // 拷贝智能指针，延长生命周期

    if (conn)
    {
        // 设置空回调，防止TcpConnection销毁时触发野指针回调
        CloseCallback emptyCallback;
        conn->setCloseCallback(emptyCallback);
        // 主动关闭连接：关闭写端，触发TCP四次挥手
        loop_->runInLoop(std::bind(&TcpConnection::shutdown, conn));
    }
    else
    {
        // 未建立有效连接，直接停止连接器即可
        connector_->stop();
    }
}

// 对外提供的：发起连接接口
void TcpClient::connect()
{
    LOG_INFO("TcpClient::connect [%s] - start connecting to %s", 
             name_.c_str(), connector_->getServerAddr().toIpPort().c_str());
    connect_ = true;
    setState(kConnecting);
    connector_->start(); // 调用Connector发起连接
}

// 对外提供的：断开连接接口
void TcpClient::disconnect()
{
    connect_ = false;
    loop_->runInLoop(std::bind(&TcpClient::removeConnection, this, connection_));
}

// 对外提供的：停止客户端接口
void TcpClient::stop()
{
    connect_ = false;
    connector_->stop();
}

// Connector连接成功后的回调函数，由Connector调用
// Connector拿到可用的sockfd后，回调到这里，创建TcpConnection
void TcpClient::newConnection(int sockfd)
{
    if (loop_->isInLoopThread()) {
        InetAddress peerAddr;
        sockets::getPeerAddr(sockfd, &peerAddr); // 从fd获取对端地址(服务器地址)
        std::string connName = getConnectionName(name_); // 生成唯一连接名

        InetAddress localAddr;
        sockets::getLocalAddr(sockfd, &localAddr); // 从fd获取本地地址

        // 创建TCP连接对象，管理fd的生命周期和读写事件
        std::shared_ptr<TcpConnection> conn(
            new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr)
        );
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);
        // 绑定连接断开的回调：断开后触发重连/清理
        conn->setCloseCallback(
            std::bind(&TcpClient::removeConnection, this, std::placeholders::_1)
        );
        LOG_INFO("TcpClient::newConnection [%s] - new connection [%s] from %s",
                name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
        
        connection_ = conn;
        setState(kConnected);
        // 连接建立成功，触发业务层的连接回调
        conn->connectEstablished();
    }
    else 
    {
        LOG_FATAL("TcpClient::newConnection - not in loop thread");
    }
    
}

// ✅ 核心：连接断开/异常后的清理+重连逻辑
void TcpClient::removeConnection(const std::shared_ptr<TcpConnection>& conn)
{
    if (loop_->isInLoopThread()) {
        LOG_INFO("TcpClient::removeConnection [%s] - connection [%s] is down",
                name_.c_str(), conn->name().c_str());

        // 校验当前连接是否是有效连接
        if (connection_ == conn)
        {
            connection_.reset();
            setState(kDisconnected);
        }

        // 连接断开后：如果开启自动重连 + 客户端未主动停止，则发起重连
        if (retry_ && connect_)
        {
            LOG_INFO("TcpClient::removeConnection [%s] - retry connecting to %s",
                    name_.c_str(), connector_->getServerAddr().toIpPort().c_str());
            setState(kConnecting);
            // 复用你写的Connector的重连逻辑，无需自己实现
            connector_->restart();
        }
        else
        {
            // 不重连则停止Connector
            connector_->stop();
        }
    }
    else 
    {
        LOG_FATAL("TcpClient::removeConnection - not in loop thread");
    }
}