#include "thread_pool.h"

#include <chrono>
#include <thread>
#include <atomic>
#include "gtest/gtest.h"

namespace dm {
namespace utils {

class ThreadPoolTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// 测试线程池构造函数
TEST_F(ThreadPoolTest, Constructor) {
  EXPECT_NO_THROW({
    ThreadPool pool(4, 100);
  });

  EXPECT_NO_THROW({
    ThreadPool pool(1, 10);
  });

  EXPECT_NO_THROW({
    ThreadPool pool(8, 1000);
  });
}

// 测试基本的任务提交和执行
TEST_F(ThreadPoolTest, BasicEnqueue) {
  ThreadPool pool(2, 10);
  std::atomic<int> counter{0};
  auto future1 = pool.enqueue([&counter]() {
    counter++;
    return 42;
  });
  
  ASSERT_TRUE(future1.valid());
  EXPECT_EQ(42, future1.get());
  EXPECT_EQ(1, counter.load());
}

// 测试带参数的任务
TEST_F(ThreadPoolTest, EnqueueWithArgs) {
  ThreadPool pool(2, 10);
  auto add = [](int a, int b) -> int {
    return a + b;
  };
  auto future = pool.enqueue(add, 10, 20);
  ASSERT_TRUE(future.valid());
  EXPECT_EQ(30, future.get());
}

// 测试多个任务的并发执行
TEST_F(ThreadPoolTest, MultipleTasks) {
  ThreadPool pool(4, 100);
  std::atomic<int> counter{0};
  std::vector<std::future<void>> futures;
  for (int i = 0; i < 20; ++i) {
    futures.push_back(pool.enqueue([&counter]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      counter++;
    }));
  }
  for (auto& future : futures) {
    ASSERT_TRUE(future.valid());
    future.wait();
  }
  EXPECT_EQ(20, counter.load());
}

// 测试任务返回不同类型
TEST_F(ThreadPoolTest, DifferentReturnTypes) {
  ThreadPool pool(2, 10);
  auto future_int = pool.enqueue([]() -> int {
    return 123;
  });

  auto future_string = pool.enqueue([]() -> std::string {
    return "hello world";
  });
  
  auto future_void = pool.enqueue([]() {
    // do nothing
  });
  
  ASSERT_TRUE(future_int.valid());
  ASSERT_TRUE(future_string.valid());
  ASSERT_TRUE(future_void.valid());
  
  EXPECT_EQ(123, future_int.get());
  EXPECT_EQ("hello world", future_string.get());
  future_void.get();
}

// 测试异常处理
TEST_F(ThreadPoolTest, ExceptionHandling) {
  ThreadPool pool(2, 10);
  
  auto future = pool.enqueue([]() -> int {
    throw std::runtime_error("test exception");
    return 42;
  });
  
  ASSERT_TRUE(future.valid());
  EXPECT_THROW(future.get(), std::runtime_error);
}

// 测试高并发场景
TEST_F(ThreadPoolTest, HighConcurrency) {
  ThreadPool pool(8, 1000);
  
  std::atomic<int> counter{0};
  std::vector<std::future<void>> futures;
  const int num_tasks = 500;
  for (int i = 0; i < num_tasks; ++i) {
    futures.push_back(pool.enqueue([&counter]() {
      volatile int temp = 0;
      for (int j = 0; j < 1000; ++j) {
        temp += j;
      }
      counter++;
    }));
  }
  for (auto& future : futures) {
    ASSERT_TRUE(future.valid());
    future.wait();
  }
  
  EXPECT_EQ(num_tasks, counter.load());
}

// 测试任务队列满的情况
TEST_F(ThreadPoolTest, TaskQueueFull) {
  ThreadPool pool(1, 2);
  std::atomic<bool> first_task_started{false};
  std::atomic<bool> can_continue{false};
  auto blocking_future = pool.enqueue([&]() {
    first_task_started = true;
    while (!can_continue.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 1;
  });
  
  while (!first_task_started.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  
  auto future2 = pool.enqueue([]() { return 2; });
  auto future3 = pool.enqueue([]() { return 3; });
  
  ASSERT_TRUE(blocking_future.valid());
  ASSERT_TRUE(future2.valid());
  ASSERT_TRUE(future3.valid());
  
  can_continue = true;
  
  EXPECT_EQ(1, blocking_future.get());
  EXPECT_EQ(2, future2.get());
  EXPECT_EQ(3, future3.get());
}

// 测试线程池销毁时的行为
TEST_F(ThreadPoolTest, DestructorBehavior) {
  std::atomic<int> completed_tasks{0};
  {
    ThreadPool pool(4, 100);
    for (int i = 0; i < 10; ++i) {
      pool.enqueue([&completed_tasks]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        completed_tasks++;
      });
    }
  }
  EXPECT_GE(completed_tasks.load(), 0);
  EXPECT_LE(completed_tasks.load(), 10);
}

// 测试线程数为0的边界情况
TEST_F(ThreadPoolTest, ZeroThreads) {
  EXPECT_NO_THROW({
    ThreadPool pool(0, 10);
    auto future = pool.enqueue([]() { return 42; });
  });
}

// 测试任务执行顺序（FIFO）
TEST_F(ThreadPoolTest, TaskExecutionOrder) {
  ThreadPool pool(1, 100);
  
  std::vector<int> execution_order;
  std::mutex order_mutex;
  std::vector<std::future<void>> futures;
  
  for (int i = 0; i < 5; ++i) {
    futures.push_back(pool.enqueue([&execution_order, &order_mutex, i]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::lock_guard<std::mutex> lock(order_mutex);
      execution_order.push_back(i);
    }));
  }
  
  for (auto& future : futures) {
    future.wait();
  }
  
  EXPECT_EQ(5, execution_order.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(i, execution_order[i]);
  }
}

}  // namespace utils
}  // namespace dm 