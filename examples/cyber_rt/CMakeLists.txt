cmake_minimum_required(VERSION 3.10)
project(BoundedQueueTest)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找需要的包
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# 尝试找到Google Test
find_package(GTest QUIET)

# 添加可执行文件
add_executable(bounded_queue_test bounded_queue_test.cc)
add_executable(thread_pool_test thread_pool_test.cc)

# 链接库
target_link_libraries(bounded_queue_test GTest::GTest GTest::Main Threads::Threads)
target_link_libraries(thread_pool_test GTest::GTest GTest::Main Threads::Threads)

# 包含当前目录
target_include_directories(bounded_queue_test PRIVATE .)
target_include_directories(thread_pool_test PRIVATE .)

# 启用测试
enable_testing()
add_test(NAME BoundedQueueTests COMMAND bounded_queue_test)
add_test(NAME ThreadPoolTests COMMAND thread_pool_test) 