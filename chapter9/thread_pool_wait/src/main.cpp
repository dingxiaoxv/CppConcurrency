#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "thread_pool.h"

// 测试函数：计算斐波那契数列
int fibonacci(int n) {
  if (n <= 1) return n;
  return fibonacci(n - 1) + fibonacci(n - 2);
}

// 测试函数：模拟耗时计算
double heavy_computation(int id, double base) {
  std::this_thread::sleep_for(std::chrono::milliseconds(100 + id * 50));
  return base * id * id;
}

// 测试函数：可能抛出异常的函数
int risky_function(int value) {
  if (value < 0) {
    throw std::invalid_argument("负数不被支持");
  }
  if (value == 42) {
    throw std::runtime_error("特殊值42触发错误");
  }
  return value * value;
}

void test_basic_tasks(thread_pool& pool) {
  std::cout << "\n=== 测试基础任务提交和等待 ===" << std::endl;

  // 提交无返回值的任务
  auto future1 = pool.submit([]() {
    std::cout << "任务1开始执行 [线程ID: " << std::this_thread::get_id() << "]"
              << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "任务1执行完成" << std::endl;
  });

  // 提交有返回值的任务
  auto future2 = pool.submit([]() -> int {
    std::cout << "任务2开始计算 [线程ID: " << std::this_thread::get_id() << "]"
              << std::endl;
    int result = 0;
    for (int i = 1; i <= 100; ++i) {
      result += i;
    }
    std::cout << "任务2计算完成，结果: " << result << std::endl;
    return result;
  });

  // 等待任务完成
  future1.wait();
  std::cout << "任务1等待完成" << std::endl;

  int result = future2.get();
  std::cout << "任务2返回值: " << result << std::endl;
}

void test_fibonacci_tasks(thread_pool& pool) {
  std::cout << "\n=== 测试斐波那契计算任务 ===" << std::endl;

  std::vector<std::future<int>> futures;
  std::vector<int> inputs = {25, 30, 35, 28, 32};

  // 提交多个斐波那契计算任务
  for (size_t i = 0; i < inputs.size(); ++i) {
    int n = inputs[i];
    auto future = pool.submit([n, i]() -> int {
      auto start = std::chrono::high_resolution_clock::now();
      std::cout << "开始计算fibonacci(" << n << ") [任务" << i + 1 << "]"
                << std::endl;

      int result = fibonacci(n);

      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      std::cout << "fibonacci(" << n << ") = " << result
                << " [用时: " << duration.count() << "ms, 任务" << i + 1 << "]"
                << std::endl;

      return result;
    });
    futures.push_back(std::move(future));
  }

  // 等待所有任务完成并收集结果
  std::cout << "等待所有斐波那契计算完成..." << std::endl;
  for (size_t i = 0; i < futures.size(); ++i) {
    int result = futures[i].get();
    std::cout << "斐波那契任务" << i + 1 << "最终结果: " << result << std::endl;
  }
}

void test_heavy_computation_tasks(thread_pool& pool) {
  std::cout << "\n=== 测试耗时计算任务 ===" << std::endl;

  std::vector<std::future<double>> futures;

  // 提交多个耗时计算任务
  for (int i = 1; i <= 5; ++i) {
    auto future =
        pool.submit([i]() -> double { return heavy_computation(i, 3.14159); });
    futures.push_back(std::move(future));
  }

  // 使用超时等待
  std::cout << "使用超时等待检查任务状态..." << std::endl;
  for (size_t i = 0; i < futures.size(); ++i) {
    auto status = futures[i].wait_for(std::chrono::milliseconds(50));
    if (status == std::future_status::ready) {
      std::cout << "任务" << i + 1 << "已完成" << std::endl;
    } else {
      std::cout << "任务" << i + 1 << "仍在执行中..." << std::endl;
    }
  }

  // 等待所有任务完成
  std::cout << "等待所有耗时计算完成..." << std::endl;
  for (size_t i = 0; i < futures.size(); ++i) {
    double result = futures[i].get();
    std::cout << "耗时计算任务" << i + 1 << "结果: " << result << std::endl;
  }
}

void test_exception_handling(thread_pool& pool) {
  std::cout << "\n=== 测试异常处理 ===" << std::endl;

  std::vector<std::future<int>> futures;
  std::vector<int> test_values = {5, -3, 42, 10, 0};

  // 提交可能抛出异常的任务
  for (int value : test_values) {
    auto future = pool.submit([value]() -> int {
      std::cout << "处理值: " << value
                << " [线程ID: " << std::this_thread::get_id() << "]"
                << std::endl;
      return risky_function(value);
    });
    futures.push_back(std::move(future));
  }

  // 逐个获取结果并处理异常
  for (size_t i = 0; i < futures.size(); ++i) {
    try {
      int result = futures[i].get();
      std::cout << "值 " << test_values[i] << " 的平方: " << result
                << std::endl;
    } catch (const std::exception& e) {
      std::cout << "值 " << test_values[i] << " 处理时发生异常: " << e.what()
                << std::endl;
    }
  }
}

void test_wait_vs_get(thread_pool& pool) {
  std::cout << "\n=== 测试wait()与get()的区别 ===" << std::endl;

  // 提交一个返回void的任务
  auto future_void = pool.submit([]() {
    std::cout << "void任务开始执行..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "void任务执行完成" << std::endl;
  });

  // 提交一个返回int的任务
  auto future_int = pool.submit([]() -> int {
    std::cout << "int任务开始执行..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    std::cout << "int任务执行完成" << std::endl;
    return 123;
  });

  // 使用wait()等待void任务
  std::cout << "使用wait()等待void任务..." << std::endl;
  future_void.wait();
  std::cout << "void任务等待完成" << std::endl;

  // 使用get()获取int任务结果
  std::cout << "使用get()获取int任务结果..." << std::endl;
  int result = future_int.get();
  std::cout << "int任务结果: " << result << std::endl;
}

int main() {
  try {
    std::cout << "创建线程池..." << std::endl;
    thread_pool pool;
    std::cout << "线程池创建成功，硬件并发线程数: "
              << std::thread::hardware_concurrency() << std::endl;

    // 运行各种测试
    test_basic_tasks(pool);
    test_fibonacci_tasks(pool);
    test_heavy_computation_tasks(pool);
    test_exception_handling(pool);
    test_wait_vs_get(pool);

    std::cout << "\n=== 所有测试完成 ===" << std::endl;
    std::cout << "等待线程池清理..." << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "发生异常: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "程序执行完成！" << std::endl;
  return 0;
}
