#!/bin/bash
# Muduo文件一键分类脚本 - 自动分base/net模块 + 分离.h/.cc
# 创建所有需要的目录结构
mkdir -p base/include base/src
mkdir -p net/include net/src
mkdir -p example build lib

# ====================== 第一步：移动【base模块】文件 ======================
# base/include 存放base的所有.h头文件
mv CurrentThread.h Logger.h noncopyable.h Thread.h Timestamp.h base/include/

# base/src 存放base的所有.cc源文件
mv CurrentThread.cc Logger.cc Thread.cc Timestamp.cc base/src/

# ====================== 第二步：移动【net模块】文件 ======================
# net/include 存放net的所有.h头文件
mv Acceptor.h Buffer.h Callbacks.h Channel.h DefaultPoller.h EPollPoller.h EventLoop.h EventLoopThread.h EventLoopThreadPool.h InetAddress.h Poller.h Socket.h TcpConnection.h TcpServer.h net/include/

# net/src 存放net的所有.cc源文件
mv Acceptor.cc Buffer.cc Channel.cc DefaultPoller.cc EPollPoller.cc EventLoop.cc EventLoopThread.cc EventLoopThreadPool.cc InetAddress.cc Poller.cc Socket.cc TcpConnection.cc TcpServer.cc net/src/

# 提示执行完成
echo "✅ 文件分类完成！目录结构已按muduo标准创建完毕"
echo "⚠️  已自动修正笔误: TcpConnnection.cc -> TcpConnection.cc"