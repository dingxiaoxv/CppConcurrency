#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <set>
#include <thread>
#include <vector>

#include "lock_free_queue.h"

// 基本功能测试
void test_basic_operations() {
  std::cout << "=== 基本功能测试 ===" << std::endl;

  lock_free_queue<int> queue;

  // 测试空队列 pop
  auto empty_result = queue.pop();
  assert(empty_result == nullptr);
  std::cout << "✓ 空队列 pop 测试通过" << std::endl;

  // 测试基本 push 和 pop
  queue.push(42);
  queue.push(100);
  queue.push(200);

  auto result1 = queue.pop();
  assert(result1 != nullptr && *result1 == 42);
  std::cout << "✓ 基本 push/pop 测试通过" << std::endl;

  auto result2 = queue.pop();
  assert(result2 != nullptr && *result2 == 100);

  auto result3 = queue.pop();
  assert(result3 != nullptr && *result3 == 200);

  // 再次测试空队列
  auto empty_result2 = queue.pop();
  assert(empty_result2 == nullptr);
  std::cout << "✓ FIFO 顺序测试通过" << std::endl;
}

// 单生产者单消费者测试
void test_single_producer_single_consumer() {
  std::cout << "=== 单生产者单消费者测试 ===" << std::endl;

  lock_free_queue<int> queue;
  const int num_items = 10000;
  std::atomic<bool> producer_done{false};
  std::vector<int> consumed_items;

  // 生产者线程
  std::thread producer([&queue, &producer_done]() {
    for (int i = 0; i < num_items; ++i) {
      queue.push(i);
    }
    producer_done.store(true);
  });

  // 消费者线程
  std::thread consumer([&queue, &consumed_items, &producer_done]() {
    int consumed_count = 0;
    while (!producer_done.load() || consumed_count < num_items) {
      auto item = queue.pop();
      if (item != nullptr) {
        consumed_items.push_back(*item);
        consumed_count++;
      }
    }
  });

  producer.join();
  consumer.join();

  // 验证结果
  assert(consumed_items.size() == num_items);
  std::sort(consumed_items.begin(), consumed_items.end());
  for (int i = 0; i < num_items; ++i) {
    assert(consumed_items[i] == i);
  }

  std::cout << "✓ 单生产者单消费者测试通过，处理了 " << num_items << " 个项目"
            << std::endl;
}

// 多生产者多消费者测试
void test_multiple_producers_consumers() {
  std::cout << "=== 多生产者多消费者测试 ===" << std::endl;

  lock_free_queue<int> queue;
  const int num_producers = 4;
  const int num_consumers = 3;
  const int items_per_producer = 2500;
  const int total_items = num_producers * items_per_producer;

  std::atomic<int> producers_done{0};
  std::atomic<int> total_consumed{0};
  std::vector<std::set<int>> consumed_by_thread(num_consumers);

  // 生产者线程
  std::vector<std::thread> producers;
  for (int p = 0; p < num_producers; ++p) {
    producers.emplace_back([&queue, &producers_done, p]() {
      for (int i = 0; i < items_per_producer; ++i) {
        int value = p * items_per_producer + i;
        queue.push(value);
      }
      producers_done.fetch_add(1);
    });
  }

  // 消费者线程
  std::vector<std::thread> consumers;
  for (int c = 0; c < num_consumers; ++c) {
    consumers.emplace_back(
        [&queue, &producers_done, &total_consumed, &consumed_by_thread, c]() {
          int local_consumed = 0;
          while (producers_done.load() < num_producers ||
                 local_consumed < total_items / num_consumers + 1000) {
            auto item = queue.pop();
            if (item != nullptr) {
              consumed_by_thread[c].insert(*item);
              local_consumed++;
              total_consumed.fetch_add(1);
            }
            if (total_consumed.load() >= total_items) break;
          }
        });
  }

  // 等待所有线程完成
  for (auto& producer : producers) {
    producer.join();
  }
  for (auto& consumer : consumers) {
    consumer.join();
  }

  // 验证结果
  std::set<int> all_consumed;
  for (const auto& thread_set : consumed_by_thread) {
    all_consumed.insert(thread_set.begin(), thread_set.end());
  }

  assert(all_consumed.size() == total_items);
  for (int i = 0; i < total_items; ++i) {
    assert(all_consumed.count(i) == 1);
  }

  std::cout << "✓ 多生产者多消费者测试通过，" << num_producers << " 个生产者，"
            << num_consumers << " 个消费者，总计处理 " << total_items
            << " 个项目" << std::endl;
}

// 压力测试
void test_stress() {
  std::cout << "=== 压力测试 ===" << std::endl;

  lock_free_queue<int> queue;
  const int num_threads = std::thread::hardware_concurrency();
  const int operations_per_thread = 50000;

  std::atomic<int> push_count{0};
  std::atomic<int> pop_count{0};

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;

  // 创建混合操作的线程
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&queue, &push_count, &pop_count, t]() {
      std::random_device rd;
      std::mt19937 gen(rd() + t);
      std::uniform_int_distribution<> dis(0, 1);

      for (int i = 0; i < operations_per_thread; ++i) {
        if (dis(gen) == 0) {
          // Push 操作
          queue.push(t * operations_per_thread + i);
          push_count.fetch_add(1);
        } else {
          // Pop 操作
          auto item = queue.pop();
          if (item != nullptr) {
            pop_count.fetch_add(1);
          }
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 清空剩余队列
  int remaining = 0;
  while (auto item = queue.pop()) {
    remaining++;
    pop_count.fetch_add(1);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  std::cout << "✓ 压力测试完成" << std::endl;
  std::cout << "  线程数: " << num_threads << std::endl;
  std::cout << "  Push 操作: " << push_count.load() << std::endl;
  std::cout << "  Pop 操作: " << pop_count.load() << std::endl;
  std::cout << "  剩余项目: " << remaining << std::endl;
  std::cout << "  用时: " << duration.count() << " 毫秒" << std::endl;
  std::cout << "  操作速度: "
            << (push_count.load() + pop_count.load()) * 1000.0 /
                   duration.count()
            << " 操作/秒" << std::endl;

  assert(push_count.load() == pop_count.load());
}

// 性能基准测试
void test_performance() {
  std::cout << "=== 性能基准测试 ===" << std::endl;

  lock_free_queue<int> queue;
  const int num_operations = 1000000;

  // 测试纯 push 性能
  auto start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < num_operations; ++i) {
    queue.push(i);
  }
  auto end_time = std::chrono::high_resolution_clock::now();
  auto push_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  // 测试纯 pop 性能
  start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < num_operations; ++i) {
    auto item = queue.pop();
    assert(item != nullptr);
  }
  end_time = std::chrono::high_resolution_clock::now();
  auto pop_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  std::cout << "✓ 性能基准测试完成" << std::endl;
  std::cout << "  Push 性能: "
            << num_operations * 1000000.0 / push_duration.count() << " 操作/秒"
            << std::endl;
  std::cout << "  Pop 性能: "
            << num_operations * 1000000.0 / pop_duration.count() << " 操作/秒"
            << std::endl;
}

// 自定义类型测试
struct CustomData {
  int id;
  std::string name;
  double value;

  CustomData(int i, const std::string& n, double v)
      : id(i), name(n), value(v) {}

  bool operator==(const CustomData& other) const {
    return id == other.id && name == other.name && value == other.value;
  }
};

void test_custom_type() {
  std::cout << "=== 自定义类型测试 ===" << std::endl;

  lock_free_queue<CustomData> queue;

  CustomData data1(1, "test1", 3.14);
  CustomData data2(2, "test2", 2.71);
  CustomData data3(3, "test3", 1.41);

  queue.push(data1);
  queue.push(data2);
  queue.push(data3);

  auto result1 = queue.pop();
  assert(result1 != nullptr && *result1 == data1);

  auto result2 = queue.pop();
  assert(result2 != nullptr && *result2 == data2);

  auto result3 = queue.pop();
  assert(result3 != nullptr && *result3 == data3);

  auto empty_result = queue.pop();
  assert(empty_result == nullptr);

  std::cout << "✓ 自定义类型测试通过" << std::endl;
}

int main() {
  std::cout << "开始 lock_free_queue 测试..." << std::endl;
  std::cout << "硬件并发数: " << std::thread::hardware_concurrency()
            << std::endl;
  std::cout << std::endl;

  try {
    test_basic_operations();
    std::cout << std::endl;

    test_single_producer_single_consumer();
    std::cout << std::endl;

    test_multiple_producers_consumers();
    std::cout << std::endl;

    test_stress();
    std::cout << std::endl;

    test_performance();
    std::cout << std::endl;

    test_custom_type();
    std::cout << std::endl;

    std::cout << "🎉 所有测试通过！lock_free_queue 工作正常。" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "❌ 测试失败: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
