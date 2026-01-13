#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装Socket地址类型（Ip地址和端口）
class InetAddress 
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr) : addr_(addr) {}

    // 这里const在后面说明该成员函数不会对类中的任何非mutable成员变量进行修改
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const {return &addr_;}
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }
private:
    sockaddr_in addr_;
};