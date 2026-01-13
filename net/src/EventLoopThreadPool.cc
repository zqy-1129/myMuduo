#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{

}
EventLoopThreadPool::~EventLoopThreadPool()
{
    // 这里不需要delete loop，因为是一个栈上的对象（EventLoopThread），其循环也不会跳出其作用域
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; i++) 
    {   
        // 使用线程池的名字加上下标
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        // 这里是获取一个线程，但是线程获取后还没有真正的执行loop
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        // 只有调用startLoop()之后loop才真正的被启动
        loops_.push_back(t->startLoop());
    }

    // 整个服务端只有一个线程，运行着和baseLoop
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}


EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    // 轮询获取loop
    if (!loops_.empty())
    {
        loop = loops_[next_];
        next_++;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}