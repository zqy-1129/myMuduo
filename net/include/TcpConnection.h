#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory> 
#include <atomic>
#include <string>

class Channel;
class EventLoop;
class Socket;

/**
 *  TcpSercer通过使用Acceptor当有新用户连接的时候，使用accept函数拿到connfd
 *  TcpConnection设置回调给Channel，Channel通知Poller需要监听的事件，
 *  Poller监听到之后又返回给Channel调用相应的回调
 */

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{ 
public:
    TcpConnection(EventLoop *loop,
        const std::string &nameArg,
        int sockfd,
        const InetAddress& localAddr,
        const InetAddress& peerAddr);

    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb) 
    { closeCallback_ = cb; }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

    void send(const std::string &buf);
    void shutdown();

private:
    enum StateE { kDisconnected, kDisconnecting, kConnected, kConnecting };
    void setState(StateE state) { state_ = state; } 

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLooop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_;  // 这里不是baseLoop，因为TcpConnection都是在subLoop中管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};