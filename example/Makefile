# testServer :
# 	g++ -o testServer testServer.cc -lmyMuduo -lpthread -g

# clean :
# 	rm -rf testServer

# testClient :
# 	g++ -o testClient testClient.cc -lmyMuduo -lpthread -g

# clean :
# 	rm -rf testClient

# 编译选项：C++11标准 + 调试信息 + 消除部分警告
CXX := g++
CXXFLAGS := -std=c++11 -g -Wall -Wno-unused-variable
# 链接选项：必须链接 自己的myMuduo库 + 线程库pthread (核心！缺一不可)
LDFLAGS := -lmyMuduo -lpthread

# 目标1：编译回声服务器 testServer
testServer: testServer.cc
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 目标2：编译测试客户端 testClient
testClient: testClient.cc
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 目标3：一键编译 服务端+客户端
all: testServer testClient

# 目标4：一键清理编译产物
clean:
	rm -rf testServer testClient *.o