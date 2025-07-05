#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "function_wrapper.h"
#include "join_threads.h"
#include "threadsafe_queue.h"
#include "work_stealing_queue.h"

class thread_pool {
 public:
  thread_pool() : done_(false), joiner_(threads_) {
    unsigned const thread_count = std::thread::hardware_concurrency();
    try {
      for (unsigned i = 0; i < thread_count; ++i) {
        queues_.push_back(
            std::make_unique<work_stealing_queue<function_wrapper>>());
      }

      for (unsigned i = 0; i < thread_count; ++i) {
        threads_.push_back(std::thread(&thread_pool::worker_thread, this, i));
      }
    } catch (...) {
      done_ = true;
      throw;
    }
  }
  ~thread_pool() { done_ = true; }

  template <typename FunctionType>
  std::future<typename std::invoke_result<FunctionType>::type> submit(
      FunctionType f) {
    using result_type = typename std::invoke_result<FunctionType>::type;
    std::packaged_task<result_type()> task(std::move(f));
    std::future<result_type> result(task.get_future());

    if (done_) {
      return std::future<result_type>();
    }

    if (local_work_queue_) {
      local_work_queue_->push(std::move(task));
      // std::cout << "push to thread " << index_ << " local queue" << std::endl;
    } else {
      pool_work_queue_.push(std::move(task));
      // std::cout << "push to thread " << index_ << " pool queue" << std::endl;
    }

    return result;
  }

  void run_pending_task() {
    function_wrapper task;
    // 优先从当前线程的专属任务队列中获取任务
    // 如果当前线程的专属任务队列为空, 则从全局任务队列中获取任务
    // 如果全局任务队列为空, 则从其他线程的专属任务队列中获取任务
    if (pop_task_from_local_queue(task) || pop_task_from_pool_queue(task) ||
        pop_task_from_other_thread_queue(task)) {
      task();
    } else {
      std::this_thread::yield();
    }
  }

 private:
  void worker_thread(size_t index) {
    index_ = index;
    local_work_queue_ = queues_[index].get();
    while (!done_) {
      run_pending_task();
    }
  }

  bool pop_task_from_local_queue(function_wrapper& task) {
    return local_work_queue_ && local_work_queue_->try_pop(task);
  }

  bool pop_task_from_pool_queue(function_wrapper& task) {
    return pool_work_queue_.try_pop(task);
  }

  bool pop_task_from_other_thread_queue(function_wrapper& task) {
    for (size_t i = 0; i < queues_.size(); ++i) {
      size_t index = (index_ + i + 1) %
                     queues_.size();  // 从下一个线程的专属任务队列中获取任务
      if (queues_[index]->try_steal(task)) {
        return true;
      }
    }
    return false;
  }

 private:
  std::atomic<bool> done_;
  threadsafe_queue<function_wrapper>
      pool_work_queue_;  // 全局任务队列, 被所有线程共享
  std::vector<std::unique_ptr<work_stealing_queue<function_wrapper>>>
      queues_;  // 线程专属任务队列, 被每个线程持有
  std::vector<std::thread> threads_;
  join_threads joiner_;
  static thread_local work_stealing_queue<function_wrapper>*
      local_work_queue_;              // 当前线程的专属任务队列
  static thread_local size_t index_;  // 当前线程的索引
};

inline thread_local work_stealing_queue<function_wrapper>*
    thread_pool::local_work_queue_ = nullptr;
inline thread_local size_t thread_pool::index_ = 0;