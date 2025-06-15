cmake_minimum_required(VERSION 3.10)
project(threadsafe_list)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")

# 启用调试模式
set(CMAKE_BUILD_TYPE Debug)

# 生成 compile_commands.json 以便 clangd 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加可执行文件
add_executable(threadsafe_list src/main.cpp)

# 链接pthread库
find_package(Threads REQUIRED)
target_link_libraries(threadsafe_list PRIVATE Threads::Threads)
