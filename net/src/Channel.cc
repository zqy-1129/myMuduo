#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

// 这就是检测的事件（EPOLLIN读事件 EPOLLOUT写事件）
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd) 
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{
}

Channel::~Channel()
{
    /**
     * muduo库中有进行断言判断是否，当我们析构的时候还有对应的事件在处理
     * 是否还有被添加到循环中，是否Loop上有该channel
     */
}

// 将连接和channel绑定在一起，防止channel被手动remove掉，
void Channel::tie(const std::shared_ptr<void>& obj) 
{   
    // 这里就是将weak_ptr与shared_ptr绑定
    tie_ = obj;
    tied_ = true;
}

/**
 * 当改变channel所表示fd的events事件后，
 * update负责在poller里面更改fd相应的事件
 * （epoll_ctl add mod del）
 * EventLoop => ChannelList Poller
 * */ 

// 通过channel所属的EventLoop，调用poller的对应方法，注册fd的events事件
void Channel::update() 
{
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove() 
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else 
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) 
{   

    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_) 
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT) 
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
