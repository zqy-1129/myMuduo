cmake_minimum_required(VERSION 2.5)
project(myMuduo)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 包含头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}/base/include
    ${PROJECT_SOURCE_DIR}/net/include
)

# 定义需要编译的文件
aux_source_directory(${PROJECT_SOURCE_DIR}/base/src BASE_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/net/src NET_SRC)

set(SRC_LIST ${BASE_SRC} ${NET_SRC})

# 编译生成动态库
add_library(myMuduo SHARED ${SRC_LIST})