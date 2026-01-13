#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        // LOG_INFO("Poller::newDefaultPoller use Poll \n");
        return nullptr;
    }
    else 
    {
        // LOG_INFO("Poller::newDefaultPoller use Epoll \n");
        return new EPollPoller(loop);
    }
}