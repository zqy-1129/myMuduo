#include <errno.h>
#include <unistd.h>
#include <cstring>

#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"


// 标识一个channel的状态
// channel未添加到poller
const int kNew = -1;    // channel的成员index_ = -1;
// channel已经添加到poller
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)   // vector<epoll_event>
{
    if (epollfd_ < 0) 
    {
        LOG_FATAL("epoll_create errpe:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{   
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());
    int numEvents = ::epoll_wait(epollfd_, 
                                &*events_.begin(), 
                                static_cast<int>(events_.size()), 
                                timeoutMs);
    int saveErro  = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0) 
    {
        LOG_INFO("%d envetns happend \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else 
    {   
        // 判断是否是外部中断
        if (saveErro != EINTR)
        {
            errno = saveErro;
            LOG_ERROR("EPollPoller::Poll() error!");
        }
    }
    return now;
}

/**
 *                      EventLoop
 *     ChannelList（all）               Poller
 *                            ChannelMap <fd, channel*> (part)
 */

// channel update => EventLoop updateChannel => Poller updateChannel
void EPollPoller::updateChannel(Channel* channel)
{
    // 这里的index就是上面的三个状态（未添加、已添加、已删除）
    const int index = channel->index();
    LOG_INFO("fd=%d evetns=%d index=%d \n", channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
        if (index == kNew) 
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } 
    else    // 已经注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } 
        else 
        {   
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel)
{
    int fd = channel->fd();
    channels_.erase(fd);
    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++) 
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        // EventLoop得到poller返回所有发生事件的列表
        activeChannels->push_back(channel);
    }
}

// 更新channel，就是epoll_ctl
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0, sizeof event);
    int fd = channel->fd();
    event.events = channel->events();
    // event.data.fd = fd;
    event.data.ptr = channel; // 这里通过epolldata携带数据，联合体选的是ptr
    

    if (::epoll_ctl(epollfd_, operation, fd, & event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctr del error%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctr add/mod error%d\n", errno);
        }
    }
}