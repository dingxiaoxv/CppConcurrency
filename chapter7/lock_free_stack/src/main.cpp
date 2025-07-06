#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "lock_free_stack.h"

// 测试配置
const int NUM_THREADS = 8;
const int OPERATIONS_PER_THREAD = 10000;
const int MAX_VALUE = 1000000;

// 统计信息
std::atomic<long long> total_push_operations(0);
std::atomic<long long> total_pop_operations(0);
std::atomic<long long> successful_pop_operations(0);

// 用于收集pop出的数据
std::mutex result_mutex;
std::vector<int> popped_values;

// 生产者线程 - 只进行push操作
void producer(lock_free_stack<int>& stack, int thread_id) {
  std::random_device rd;
  std::mt19937 gen(rd() + thread_id);
  std::uniform_int_distribution<> dis(1, MAX_VALUE);

  for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
    int value = dis(gen);
    stack.push(value);
    total_push_operations++;

    // 偶尔让出CPU时间片
    if (i % 1000 == 0) {
      std::this_thread::yield();
    }
  }
}

// 消费者线程 - 只进行pop操作
void consumer(lock_free_stack<int>& stack, int thread_id) {
  (void)thread_id;  // 避免未使用参数警告
  std::vector<int> local_results;

  for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
    auto result = stack.pop();
    total_pop_operations++;

    if (result) {
      local_results.push_back(*result);
      successful_pop_operations++;
    }

    // 偶尔让出CPU时间片
    if (i % 1000 == 0) {
      std::this_thread::yield();
    }
  }

  // 将本地结果合并到全局结果
  {
    std::lock_guard<std::mutex> lock(result_mutex);
    popped_values.insert(popped_values.end(), local_results.begin(),
                         local_results.end());
  }
}

// 混合线程 - 同时进行push和pop操作
void mixed_worker(lock_free_stack<int>& stack, int thread_id) {
  std::random_device rd;
  std::mt19937 gen(rd() + thread_id);
  std::uniform_int_distribution<> dis(1, MAX_VALUE);
  std::uniform_int_distribution<> op_dis(0, 1);

  std::vector<int> local_results;

  for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
    if (op_dis(gen) == 0) {
      // Push操作
      int value = dis(gen);
      stack.push(value);
      total_push_operations++;
    } else {
      // Pop操作
      auto result = stack.pop();
      total_pop_operations++;

      if (result) {
        local_results.push_back(*result);
        successful_pop_operations++;
      }
    }

    // 偶尔让出CPU时间片
    if (i % 1000 == 0) {
      std::this_thread::yield();
    }
  }

  // 将本地结果合并到全局结果
  {
    std::lock_guard<std::mutex> lock(result_mutex);
    popped_values.insert(popped_values.end(), local_results.begin(),
                         local_results.end());
  }
}

// 性能测试
void performance_test() {
  std::cout << "\n=== 性能测试 ===" << std::endl;

  lock_free_stack<int> stack;

  // 重置统计信息
  total_push_operations = 0;
  total_pop_operations = 0;
  successful_pop_operations = 0;
  popped_values.clear();

  auto start_time = std::chrono::high_resolution_clock::now();

  // 创建生产者线程
  std::vector<std::thread> producers;
  for (int i = 0; i < NUM_THREADS / 2; ++i) {
    producers.emplace_back(producer, std::ref(stack), i);
  }

  // 创建消费者线程
  std::vector<std::thread> consumers;
  for (int i = 0; i < NUM_THREADS / 2; ++i) {
    consumers.emplace_back(consumer, std::ref(stack), i + NUM_THREADS / 2);
  }

  // 等待所有线程完成
  for (auto& t : producers) {
    t.join();
  }
  for (auto& t : consumers) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
  std::cout << "Push操作总数: " << total_push_operations << std::endl;
  std::cout << "Pop操作总数: " << total_pop_operations << std::endl;
  std::cout << "成功的Pop操作: " << successful_pop_operations << std::endl;
  std::cout << "平均吞吐量: "
            << (total_push_operations + total_pop_operations) * 1000.0 /
                   duration.count()
            << " ops/sec" << std::endl;
}

// 混合操作测试
void mixed_operations_test() {
  std::cout << "\n=== 混合操作测试 ===" << std::endl;

  lock_free_stack<int> stack;

  // 重置统计信息
  total_push_operations = 0;
  total_pop_operations = 0;
  successful_pop_operations = 0;
  popped_values.clear();

  auto start_time = std::chrono::high_resolution_clock::now();

  // 创建混合工作线程
  std::vector<std::thread> workers;
  for (int i = 0; i < NUM_THREADS; ++i) {
    workers.emplace_back(mixed_worker, std::ref(stack), i);
  }

  // 等待所有线程完成
  for (auto& t : workers) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
  std::cout << "Push操作总数: " << total_push_operations << std::endl;
  std::cout << "Pop操作总数: " << total_pop_operations << std::endl;
  std::cout << "成功的Pop操作: " << successful_pop_operations << std::endl;
  std::cout << "平均吞吐量: "
            << (total_push_operations + total_pop_operations) * 1000.0 /
                   duration.count()
            << " ops/sec" << std::endl;
}

// 正确性测试
void correctness_test() {
  std::cout << "\n=== 正确性测试 ===" << std::endl;

  lock_free_stack<int> stack;

  // 测试基本功能
  std::cout << "测试基本push和pop功能..." << std::endl;

  // Push一些数据
  std::vector<int> test_data = {1, 2, 3, 4, 5};
  for (int value : test_data) {
    stack.push(value);
  }

  // Pop数据并验证
  std::vector<int> popped_data;
  while (true) {
    auto result = stack.pop();
    if (!result) break;
    popped_data.push_back(*result);
  }

  std::cout << "Push的数据: ";
  for (int value : test_data) {
    std::cout << value << " ";
  }
  std::cout << std::endl;

  std::cout << "Pop的数据: ";
  for (int value : popped_data) {
    std::cout << value << " ";
  }
  std::cout << std::endl;

  // 验证栈的后进先出特性
  bool is_correct = true;
  if (popped_data.size() == test_data.size()) {
    for (size_t i = 0; i < test_data.size(); ++i) {
      if (popped_data[i] != test_data[test_data.size() - 1 - i]) {
        is_correct = false;
        break;
      }
    }
  } else {
    is_correct = false;
  }

  std::cout << "栈的后进先出特性: " << (is_correct ? "正确" : "错误")
            << std::endl;

  // 测试空栈pop
  auto empty_result = stack.pop();
  std::cout << "空栈pop结果: " << (empty_result ? "有值" : "空") << std::endl;
}

// 压力测试
void stress_test() {
  std::cout << "\n=== 压力测试 ===" << std::endl;

  lock_free_stack<int> stack;
  const int STRESS_THREADS = 16;
  const int STRESS_OPERATIONS = 50000;

  std::atomic<long long> stress_pushes(0);
  std::atomic<long long> stress_pops(0);
  std::atomic<long long> stress_successful_pops(0);

  auto stress_worker = [&](int thread_id) {
    std::random_device rd;
    std::mt19937 gen(rd() + thread_id);
    std::uniform_int_distribution<> dis(1, MAX_VALUE);
    std::uniform_int_distribution<> op_dis(0, 1);

    for (int i = 0; i < STRESS_OPERATIONS; ++i) {
      if (op_dis(gen) == 0) {
        stack.push(dis(gen));
        stress_pushes++;
      } else {
        auto result = stack.pop();
        stress_pops++;
        if (result) {
          stress_successful_pops++;
        }
      }
    }
  };

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> stress_threads;
  for (int i = 0; i < STRESS_THREADS; ++i) {
    stress_threads.emplace_back(stress_worker, i);
  }

  for (auto& t : stress_threads) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  std::cout << "压力测试完成！" << std::endl;
  std::cout << "线程数: " << STRESS_THREADS << std::endl;
  std::cout << "每线程操作数: " << STRESS_OPERATIONS << std::endl;
  std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
  std::cout << "Push操作: " << stress_pushes << std::endl;
  std::cout << "Pop操作: " << stress_pops << std::endl;
  std::cout << "成功Pop操作: " << stress_successful_pops << std::endl;
  std::cout << "总吞吐量: "
            << (stress_pushes + stress_pops) * 1000.0 / duration.count()
            << " ops/sec" << std::endl;
}

int main() {
  std::cout << "Lock-Free Stack 多线程高并发测试" << std::endl;
  std::cout << "================================" << std::endl;
  std::cout << "线程数: " << NUM_THREADS << std::endl;
  std::cout << "每线程操作数: " << OPERATIONS_PER_THREAD << std::endl;
  std::cout << "最大值范围: " << MAX_VALUE << std::endl;

  try {
    // 运行各种测试
    correctness_test();
    performance_test();
    mixed_operations_test();
    stress_test();

    std::cout << "\n所有测试完成！" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
