#include <myMuduo/TcpClient.h>
#include <myMuduo/EventLoop.h>
#include <myMuduo/InetAddress.h>
#include <myMuduo/Logger.h>
#include <myMuduo/Buffer.h>
#include <myMuduo/Timestamp.h>

#include <iostream>
#include <string>
#include <thread>
#include <functional>

using namespace std;

// 全局变量
EventLoop* g_loop = nullptr;
TcpConnectionPtr g_conn;

// 连接状态回调
void onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        g_conn = conn;
        LOG_INFO("【客户端】连接服务端成功 ✅");
        LOG_INFO("服务端地址: %s", conn->getLocalAddr().toIpPort().c_str());
        LOG_INFO("本地地址: %s", conn->getPeerAddr().toIpPort().c_str());
        LOG_INFO("------------------------");
        LOG_INFO("请输入消息，回车发送 (输入 quit 断开连接)");
    }
    else
    {
        g_conn.reset();
        LOG_INFO("【客户端】与服务端断开连接 ❌");
    }
}

// 消息接收回调
void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
{
    string msg = buf->retrieveAllAsString();
    LOG_INFO("【客户端收到服务端消息】: %s", msg.c_str());
    LOG_INFO("消息长度: %zu 字节 | 接收时间: %s", msg.size(), receiveTime.toString().c_str());
}

// 消息发送完成回调
void onWriteComplete(const TcpConnectionPtr& conn)
{
    LOG_INFO("【客户端】消息发送完成 ✅\n");
}

// ✅ 无参控制台输入线程函数（核心修改点）
void inputThread()
{
    string msg;
    while (true)
    {
        getline(cin, msg);
        if (msg.empty()) continue;

        if (msg == "quit")
        {
            if (g_conn && g_conn->connected())
            {
                g_conn->shutdown();
                LOG_INFO("【客户端】输入quit，主动断开连接！");
            }
            break;
        }

        if (g_conn && g_conn->connected())
        {   
            g_conn->send(msg);
            cout << msg << endl;
            LOG_INFO("【客户端发送消息】: %s", msg.c_str());
        }
        else
        {
            LOG_ERROR("⚠️  连接已断开，无法发送消息！");
        }
    }
}

int main(int argc, char* argv[])
{
    LOG_INFO("===== TCP客户端启动 =====");

    EventLoop loop;
    g_loop = &loop;

    // 你的服务端端口是8888，这里写对！
    InetAddress serverAddr(uint16_t(8888) ,"127.0.0.1");
    TcpClient client(&loop, serverAddr, "MyTestClient");

    client.enableRetry();
    client.setConnectionCallback(onConnection);
    client.setMessageCallback(onMessage);
    client.setWriteCompleteCallback(onWriteComplete);

    client.connect();

    // ✅ 正确创建无参线程，编译绝对通过
    std::thread input_thread(inputThread);
    input_thread.detach();

    loop.loop();

    LOG_INFO("===== TCP客户端正常退出 =====");
    return 0;
}