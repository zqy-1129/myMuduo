#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"

#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

class EventLoop;
class Channel;

/**
 * Connector将TcpClient建立连接和事件处理进行分离
 * Connector负责建立连接，TcpClient只负责进行管理
 * 
 * Connector持有 EventLoop、服务端 InetAddress
 * 创建非阻塞的客户端sockfd，并发起异步非阻塞的connect()系统调用
 * 通过channnel监听socket的可写事件，判断连接是否成功
 * 连接成之后，把创建好的sockfd传递给TcpClient
 */

class Connector : noncopyable
{
public:
    // 连接成功的回调，传递fd给TcpClient
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Connector(EventLoop* loop, const InetAddress& serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb) { newConnectCallback_ = cb; }

    void start();    // 发起连接
    void restart();  // 重启连接（重连用）
    void stop();     // 停止连接

    void setRetryDelayMs(int ms) { retryDelayMs_ = ms; } // 设置重试间隔

    const InetAddress getServerAddr() const { return serverAddr_; }

private:
    enum States { kDisconnected, kConnecting, kConnected };

    static const int kInitRetryDelayMs = 500;   // 默认重试间隔
    static const int kMaxRetryDelayMs = 300000; // 最大重试间隔

    void setState(States s) { state_ = s; }
    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void handleWrite();  // 监听可写事件，判断连接结果
    void handleError();  // 处理连接错误
    void retry(int sockfd); // 重试连接
    int removeAndResetChannel(); // 清理Channel
    void resetChannel();         // 重置Channel

    EventLoop* loop_;
    InetAddress serverAddr_;
    std::atomic<bool> connect_;         // 是否正在连接
    States state_;                      // 连接状态   
    std::unique_ptr<Channel> channel_;  // 客户端fd封装的channel
    NewConnectionCallback newConnectCallback_;
    int retryDelayMs_;                  // 重试间隔
};