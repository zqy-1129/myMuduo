#!/bin/bash

# 优化1: 先删除系统旧的库文件和头文件
sudo rm -rf /usr/include/myMuduo
sudo rm -f /usr/lib/libmyMuduo.so /usr/lib/libmyMuduo.a

# 优化2: 执行过程中遇到错误立即退出 (放到核心编译步骤前)
set -e

# 创建build目录并清空
if [ ! -d "`pwd`/build" ]; then
    mkdir -p `pwd`/build
fi
rm -rf `pwd`/build/*

# 编译核心步骤：cmake + make 多核编译更快
echo "开始编译 myMuduo 库..."
cd `pwd`/build && cmake .. && make -j4
cd ..

# ========== 核心修改1: 拷贝所有头文件  ==========
# 适配新目录: 把base/include 和 net/include 下的所有.h 统一复制到系统头文件目录
if [ ! -d "/usr/include/myMuduo" ]; then
    sudo mkdir -p /usr/include/myMuduo
fi
sudo cp -r ./base/include/*  /usr/include/myMuduo/
sudo cp -r ./net/include/*   /usr/include/myMuduo/



sudo cp lib/libmyMuduo.so /usr/lib/ && sudo ldconfig

echo -e "\033[32m编译安装成功！✅ myMuduo已安装到系统目录\033[0m"