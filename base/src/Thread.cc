#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDeaultName();
}

Thread::~Thread()
{   
    // 如果启动并且没有被等待
    if (started_ && !joined_)
    {   
        // thread类提供的设置分类线程的方法
        thread_->detach();
    }
}

// 一个Thread对象就是记录一个新线程的详细信息
void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    // 开启一个线程对象
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();  // 开启一个新的线程，专门执行该线程函数
    }));

    // 这里必须等待上面创建线程的tid值
    sem_wait(&sem);
}

// 阻塞并回收子线程资源 pthread_join()
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDeaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}