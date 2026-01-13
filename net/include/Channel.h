#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

// 这里使用前置声明而不是直接将头文件包含进来
// 减少了对外的头文件暴露，文件编译之后编程.so动态链接
class EventLoop;

/**
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，
 * 如EPOLLIN，EPOLLOUT事件还绑定了poller返回的具体事件
 * 实际上就是对事件的封装
 */

class Channel : noncopyable
{
public:

    // 这里使用c++11新特性，将这个定义一个名字
    // 原来的代码typedef std::function<void()> EventCallback
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // 使用指针的可以使用前置声明因为指针的大小确定，
    // 但是如果要使用实际的类型则必须引用头文件知道其具体大小
    // fd得到poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) {readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb);}

    // 防止channel被手动remove，channel还在执行回调函数操作
    void tie(const std::shared_ptr<void> &);

    int fd() const {return fd_;}
    int events() const {return events_;}
    void set_revents(int revt) {revents_ = revt;}

    // 设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ |= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ == kWriteEvent; }
    bool isReading() const { return events_ == kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }; 

    // one loop per thread
    // 当前的channel属于哪一个eventLoop（一个循环监听着多个channel）
    EventLoop* ownerLoop() { return loop_; }
    void remove();
    
private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // 这里就是fd检测的事件是什么样的事件（EPOLLIN、EPOLLOUT）
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;   // 事件循环
    const int fd_;      // poller监听的对象
    int events_;        // 注册fd感兴趣的事件
    int revents_;       // poller返回具体发生的事件
    int index_;       
    
    // 实现对象销毁后，自动跳过无效回调
    std::weak_ptr<void> tie_;  // 用于监听多线程中的状态
    bool tied_;

    // 因为channel通道中能够获知fd最终发生的具体事件revents，
    // 所以其负责条用具体事件的回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};