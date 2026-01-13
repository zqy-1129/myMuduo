#pragma once

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Logger.h"
#include "Buffer.h"

#include <functional>
#include <unordered_map>
#include <string>
#include <memory>
#include <atomic>

// 对外的服务器编程的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        KNoReusePort,
        KReusePost,
    };

    TcpServer(EventLoop *loop, 
        const InetAddress &listenAddr, 
        const std::string &nameArg, 
        Option option = KNoReusePort);

    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    // 设置工作循环的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听，也就是开启acceptor
    void start();

private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnction(const TcpConnectionPtr &conn);
    void removeConnctionInLoop(const TcpConnectionPtr & conn);

    EventLoop *loop_;  // baseLoop用户定义的loop

    const std::string ipPort_;
    const std::string name_;  // 服务器的名称

    std::unique_ptr<Acceptor> acceptor_;            // 运行在mainLoop，监听新连接

    std::shared_ptr<EventLoopThreadPool> threadPool_;  // one loop per thread

    ConnectionCallback connectionCallback_;         // 有新连接时候的回调
    MessageCallback messageCallback_;               // 有读写消息时候的回调
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_;         // loop线程初始化
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;     // 保存所有的连接
};