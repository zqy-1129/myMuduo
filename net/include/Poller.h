#pragma once

#include <vector>
#include <unordered_map>

#include "Timestamp.h"

// 这里不需要实际类的大小，仅使用指针的可以使用这种前置声明
class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO服用模块
class Poller 
{
public:
    // 一个poller对应多个channel（多路复用）
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);

    // 虚析构函数保证子类对象的正确析构
    virtual ~Poller();

    // 给所有IO复用保留统一的接口，超时时间以及感兴趣的channel（类似启动epoll_wait）
    virtual Timestamp poll(int timeoutMs, ChannelList * activeChannels) = 0;

    // 下面是类似于epoll_ctl(add/mod/del)
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前Poller中
    bool hasChannel(Channel * channel) const;

    // EventLoop可以使用该接口获得默认的IO复用对象的具体实现（epoll、poll）
    // 这里由于Poller是基类，尽量不要包含子类头文件，故不再这里实现
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    // map的key是sockfd value是sockfd所属的channel通道
    // channel是sockfd和对应感兴趣事件的封装
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_;  // 定义poller所属的事件循环EventLoop
};