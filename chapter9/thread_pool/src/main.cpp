#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "threadsafe_queue.h"

class join_threads {
 public:
  explicit join_threads(std::vector<std::thread>& threads)
      : threads_(threads) {}
  ~join_threads() {
    for (auto& thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

 private:
  std::vector<std::thread>& threads_;
};

class thread_pool {
 public:
  thread_pool() : done_(false), joiner_(threads_) {
    unsigned const thread_count = std::thread::hardware_concurrency();
    try {
      for (unsigned i = 0; i < thread_count; ++i) {
        threads_.push_back(std::thread(&thread_pool::worker_thread, this));
      }
    } catch (...) {
      done_ = true;
      throw;
    }
  }
  ~thread_pool() { done_ = true; }
  template <typename FunctionType>
  void submit(FunctionType f) {
    work_queue_.push(std::function<void()>(f));
  }

 private:
  void worker_thread() {
    while (!done_) {
      std::function<void()> task;
      if (work_queue_.try_pop(task)) {
        task();
      } else {
        std::this_thread::yield();
      }
    }
  }

 private:
  std::atomic<bool> done_;
  threadsafe_queue<std::function<void()>> work_queue_;
  std::vector<std::thread> threads_;
  join_threads joiner_;
};

int main() {
  try {
    thread_pool pool;
    std::cout << "thread pool created" << std::endl;

    pool.submit([]() { 
      std::cout << "task 1 completed" << std::endl; 
      std::this_thread::sleep_for(std::chrono::seconds(3));
    });

    pool.submit([]() { 
      std::cout << "task 2 completed" << std::endl; 
      std::this_thread::sleep_for(std::chrono::seconds(1));
    });

    pool.submit([]() { 
      std::cout << "task 3 completed" << std::endl; 
      std::this_thread::sleep_for(std::chrono::seconds(2));
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "\nthread pool test completed!" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
