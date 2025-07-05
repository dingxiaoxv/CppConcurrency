#pragma once

#include <deque>
#include <mutex>

#include "function_wrapper.h"

template<typename T>
class work_stealing_queue {
public:
  work_stealing_queue() = default;
  work_stealing_queue(const work_stealing_queue&) = delete;
  work_stealing_queue& operator=(const work_stealing_queue&) = delete;

  void push(function_wrapper&& task) {
    std::lock_guard<std::mutex> lk(mtx_);
    queue_.push_front(std::move(task));
  }

  bool try_pop(function_wrapper& task) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (queue_.empty()) {
      return false;
    }
    task = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

  bool try_steal(function_wrapper& task) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (queue_.empty()) {
      return false;
    }
    task = std::move(queue_.back());
    queue_.pop_back();
    return true;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return queue_.empty();
  }

private:
  std::deque<function_wrapper> queue_;
  mutable std::mutex mtx_;
};