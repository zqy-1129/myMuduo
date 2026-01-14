#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "TcpClient.h"
#include "TcpConnection.h"
#include "Connector.h"
#include "EventLoop.h"
#include "Logger.h"
#include "sockets.h"

#include <memory>
#include <string>
#include <functional>
#include <string>
#include <atomic>


class TcpClient
{
public:
    TcpClient(EventLoop* loop,
        const InetAddress& serverAddr,
        const std::string& nameArg);

    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    // 设置各类业务回调函数
    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

    // 是否开启自动重连(断开后自动重试连接)
    void enableRetry() { retry_ = true; }
    bool retry() const { return retry_; }

    // 获取客户端名称
    const std::string& name() const { return name_; }

private:
    // 内部私有回调，供Connector调用
    void newConnection(int sockfd);
    // 连接断开后的清理逻辑
    void removeConnection(const std::shared_ptr<TcpConnection>& conn);

    enum State { kDisconnected, kConnecting, kConnected };
    void setState(State s) { state_ = s; }

    EventLoop* loop_;                  // 所属的事件循环，必须传入
    std::unique_ptr<Connector> connector_; // 核心连接器
    const std::string name_;           // 客户端名称，日志区分用

    ConnectionCallback connectionCallback_;    // 连接成功/失败回调
    MessageCallback messageCallback_;          // 收到消息回调
    WriteCompleteCallback writeCompleteCallback_; // 数据发送完成回调
    HighWaterMarkCallback highWaterMarkCallback_;
    size_t highWaterMark_;                     // 高水位标记，用于流量控制

    std::atomic<bool> retry_;           // 是否自动重连
    std::atomic<bool> connect_;         // 是否处于连接状态
    State state_;                       // 当前连接状态

    std::shared_ptr<TcpConnection> connection_; // 保存当前的TCP连接
};