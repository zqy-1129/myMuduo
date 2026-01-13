#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>

// 防止一个线程创建多个EventLoop，__thread就是控制这个全局变量每个线程中有自己的一份（thread_local）
__thread EventLoop *t_loopInThisThread= nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来唤醒notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p int thread %d", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop都将监听wakeupchannnel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_) 
    {
        activeChannels_.clear();
        // 相当于revents存到activeChannels中
        // 这里之间两类fd，一种是client的fd，一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *Channel: activeChannels_) 
        {   
            // Poller监听那些channel发生事件，然后上报给EventLoop，通知channel处理相应的事件
            Channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop accept 只进行新用户的链接 fd 封装到channel中，
         * mainLoop 事先注册一个回调函数（需要由subloop执行）唤醒subloop执行之前mainloop注册的操作
         */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环 1.loop在自己的线程中调用quit 2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread())  // 在其他线程中调用quit，在一个subloop中，调用了mainLoop的quit
    {   
        // 若是其他要终止此线程应该先进行唤醒
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())   // 在当前loop线程中执行cb
    {   
        cb();
    }
    else   // 在非当前线程访问cb，唤醒loop所在线程执行cb
    {
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的loop的线程，callingPendingFunctors_表示正在执行回调没有阻塞在其上
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();  // 唤醒loop所在线程
    }
}

// 用来唤醒loop所在的线程，向wakeupfd_写一个数据，wakeupChannel就发生都时间，当前的loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n)
    }
}


// 调用Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::handleRead()
{   
    // 向对应的reactor发送一个消息使其唤醒
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
}

void EventLoop::doPendingFunctors()  // 执行回调函数
{   
    // 使用这个局部变量进行置换，直接一次将所有需要执行的回调全部拿出来，不耽误其在向内添加
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {   
        // 这个锁是锁作用域的除了这个括号就释放
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor & functor : functors)
    {
        functor();
    }

    callingPendingFunctors_ = false;
}