#pragma once

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"

class Channel;

/**
 * epoll的使用
 * epoll_create 创建epollfd专属epoll的文件描述符
 * epoll_ctl    更新事件add/mod/del
 * epoll_wait   开启事件循环
 */

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写基类的抽象方法
    // (epoll_wait)
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    //（epoll_ctl） 
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    // 事件集合的长度（源码默认为16）
    static const int kInitEventListSize = 16;

    // 填写活跃的链接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};