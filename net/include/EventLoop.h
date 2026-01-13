#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Channel.h"
#include "Timestamp.h"
#include "Poller.h"
#include "CurrentThread.h"

class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在线程，执行cb
    void queueInLoop(Functor cb);

    // 用于唤醒loop所在线程，main reactor唤醒sub reactor执行操作
    void wakeup();

    // EventLoop调用Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断EventLoop对象是都在自己的线程里
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();          // wake uo
    void doPendingFunctors();   // 回调


    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;  // 原子操作，通过CAS实现
    std::atomic_bool quit_;     // 标识退出loop循环
    std::atomic_bool callingPendingFunctors_;  // 标识当前loop是否有需要执行的回调
    // one loop pre thread 
    const pid_t threadId_;      // 当前loop所在线程id
    // 返回revent
    Timestamp pollReturnTime_;  // poller返回发生事件的channels的时间
    std::unique_ptr<Poller> poller_;

    // 主要作用，当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，将其唤醒处理channel
    int wakeupFd_;   // 使用的是eventfd()这个系统调用，是线程间的通信效率较高
    std::unique_ptr<Channel> wakeupChannel_;    // channel和fd进行绑定

    ChannelList activeChannels_;

    std::vector<Functor> pendingFunctors_;      // 存储loop需要执行的所有回调操作
    std::mutex mutex_;      // 用于保护上面vector容器的线程安全
}; 