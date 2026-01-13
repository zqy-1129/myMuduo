#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// 这里创建的都是非阻塞fd也就是muduo库的精髓
static int createNonBlocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonBlocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);  // bind
    // TcpServer::start() Acceptor.listen，有新用户的连接，需要指向一个回调（connfd=>channel=>subloop）
    // baseLoop => acceptChannel_(listenfd) => 
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}
Acceptor::~Acceptor()
{   
    // 将里面的连接全部删除
    acceptChannel_.disableAll();
    // 将自己remove
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();  // 监听
    acceptChannel_.enableReading();  // acceptChannel_ => Poller
}

// 每当有新用户连接就会执行，这个函数设置成channel中的回调函数
void Acceptor::handleRead()
{   
    // 这里InetAddress没有空的默认构造函数需要加一个端口号或者一个地址
    InetAddress peerAddr;
    // 建立连接得到新建的文件描述符，并将socket详细地址和端口写入peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd > 0)
    {
        if (newConnectionCallback_) 
        {
            newConnectionCallback_(connfd, peerAddr);   // 轮询找到subLoop，唤醒并分发当前的新客户端的Channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else{
        LOG_ERROR("%s:%s:%d accept error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}