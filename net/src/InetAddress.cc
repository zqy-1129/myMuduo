#include "InetAddress.h"

#include <strings.h>
#include <string.h>

// 默认参数的设置只在.h中即可，在这里加上会出现歧义
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;

    // 字节序转换，转换成网络字节序（s代表short）
    addr_.sin_port = htons(port);
    // 这里是Ip地址的字节序转换
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

// 这里const在后面说明该成员函数不会对类中的任何非mutable成员变量进行修改
std::string InetAddress::toIp() const
{
    char buf[64] = {0};
    // 将网络字节序的二进制IP地址转换为点分十进制
    // 这里的::是说明是全局作用域的这个函数避免冲突
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}

std::string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    // 将网络字节序的二进制IP地址转换为点分十进制
    // 这里的::是说明是全局作用域的这个函数避免冲突
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    // 这里是将端口号拼接到IP之后，首先是写入地址，之后是剩余缓冲区大小，接下来是格式化字符串
    snprintf(buf + end, sizeof(buf) - end, ":%u", port);
    return buf;

}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}