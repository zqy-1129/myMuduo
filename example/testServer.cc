#include <myMuduo/TcpServer.h>
#include <myMuduo/Logger.h>

#include <string>

class EchoServer
{
public:
    EchoServer(EventLoop* loop,
            const InetAddress &addr,
            const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        // 设置合适的loop线程数量
        server_.setThreadNum(4);
    }

    void start()
    {
        server_.start();
    }
private:
    EventLoop* loop_;
    TcpServer server_;

    // 连接事件回调
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else 
        {
            LOG_INFO("Connection DOWN: %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    void onMessage(const TcpConnectionPtr& conn,
            Buffer* buffer,
            Timestamp receiveTime)
    {
        std::string msg = buffer->retrieveAllAsString();
        LOG_INFO("Received %ld bytes from %s at %s \n",
            msg.size(),
            conn->peerAddress().toIpPort().c_str(),
            receiveTime.toString().c_str());
        conn->send(msg);
        conn->shutdown();
    }
};


int main()
{
    EventLoop loop;
    InetAddress addr(8888);
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
    loop.loop();
    return 0;
}