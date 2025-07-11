/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "bounded_queue.h"

namespace dm {
namespace utils {

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_num, std::size_t max_task_num = 1000);

  template <typename F, typename... Args>
  auto enqueue(F&& f, Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type>;

  ~ThreadPool();

 private:
  std::vector<std::thread> workers_;
  BoundedQueue<std::function<void()>> task_queue_;
  std::atomic_bool stop_{false};
};

inline ThreadPool::ThreadPool(std::size_t threads, std::size_t max_task_num) {
  if (!task_queue_.init(max_task_num)) {
    throw std::runtime_error("Task queue init failed.");
  }
  
  workers_.reserve(threads);
  for (size_t i = 0; i < threads; ++i) {
    workers_.emplace_back([this] {
      while (!stop_) {
        std::function<void()> task;
        if (task_queue_.waitDequeue(&task)) {
          task();
        }
      }
    });
  }
}

// before using the return value, you should check value.valid()
template <typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
  using return_type = typename std::result_of<F(Args...)>::type;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();

  // don't allow enqueueing after stopping the pool
  if (stop_) {
    return std::future<return_type>();
  }
  task_queue_.enqueue([task]() { (*task)(); });
  return res;
};

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
  if (stop_.exchange(true)) {
    return;
  }
  task_queue_.breakAllWait();
  for (std::thread& worker : workers_) {
    worker.join();
  }
}

}  // namespace utils
}  // namespace dm
