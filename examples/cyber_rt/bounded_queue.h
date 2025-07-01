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

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

namespace dm {
namespace utils {

template <typename T>
class BoundedQueue {
 public:
  using value_type = T;
  using size_type = uint64_t;

 public:
  BoundedQueue() {}
  BoundedQueue& operator=(const BoundedQueue& other) = delete;
  BoundedQueue(const BoundedQueue& other) = delete;
  ~BoundedQueue();
  bool init(uint64_t size);
  bool enqueue(const T& element);
  bool enqueue(T&& element);
  bool waitEnqueue(const T& element);
  bool waitEnqueue(T&& element);
  bool dequeue(T* element);
  bool waitDequeue(T* element);
  uint64_t size();
  bool empty();
  void breakAllWait();
  uint64_t head() { return head_.load(); }
  uint64_t tail() { return tail_.load(); }
  uint64_t commit() { return commit_.load(); }

 private:
  uint64_t getIndex(uint64_t num);

  alignas(64) std::atomic<uint64_t> head_{0};
  alignas(64) std::atomic<uint64_t> tail_{1};
  alignas(64) std::atomic<uint64_t> commit_{1};
  uint64_t pool_size_{0};
  T* pool_{nullptr};
  std::condition_variable cv_;
  std::mutex mutex_;
  std::atomic_bool break_all_wait_{false};
};

template <typename T>
BoundedQueue<T>::~BoundedQueue() {
  cv_.notify_all();
  if (pool_) {
    for (uint64_t i = 0; i < pool_size_; ++i) {
      pool_[i].~T();
    }
    std::free(pool_);
  }
}

template <typename T>
bool BoundedQueue<T>::init(uint64_t size) {
  // Head and tail each occupy a space
  pool_size_ = size + 2;
  pool_ = reinterpret_cast<T*>(std::calloc(pool_size_, sizeof(T)));
  if (pool_ == nullptr) {
    return false;
  }
  for (uint64_t i = 0; i < pool_size_; ++i) {
    new (&(pool_[i])) T();
  }
  return true;
}

template <typename T>
bool BoundedQueue<T>::enqueue(const T& element) {
  uint64_t new_tail = 0;
  uint64_t old_commit = 0;
  uint64_t old_tail = tail_.load(std::memory_order_acquire);
  do {
    new_tail = old_tail + 1;
    if (getIndex(new_tail) == getIndex(head_.load(std::memory_order_acquire))) {
      return false;
    }
  } while (!tail_.compare_exchange_weak(old_tail, new_tail,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed));
  pool_[getIndex(old_tail)] = element;
  do {
    old_commit = old_tail;
  } while (!commit_.compare_exchange_weak(
      old_commit, new_tail, std::memory_order_acq_rel,
      std::memory_order_relaxed));
  cv_.notify_one();
  return true;
}

template <typename T>
bool BoundedQueue<T>::enqueue(T&& element) {
  uint64_t new_tail = 0;
  uint64_t old_commit = 0;
  uint64_t old_tail = tail_.load(std::memory_order_acquire);
  do {
    new_tail = old_tail + 1;
    if (getIndex(new_tail) == getIndex(head_.load(std::memory_order_acquire))) {
      return false;
    }
  } while (!tail_.compare_exchange_weak(old_tail, new_tail,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed));
  pool_[getIndex(old_tail)] = std::move(element);
  do {
    old_commit = old_tail;
  } while (!commit_.compare_exchange_weak(
      old_commit, new_tail, std::memory_order_acq_rel,
      std::memory_order_relaxed));
  cv_.notify_one();
  return true;
}

template <typename T>
bool BoundedQueue<T>::dequeue(T* element) {
  uint64_t new_head = 0;
  uint64_t old_head = head_.load(std::memory_order_acquire);
  do {
    new_head = old_head + 1;
    if (new_head == commit_.load(std::memory_order_acquire)) {
      return false;
    }
    *element = pool_[getIndex(new_head)];
  } while (!head_.compare_exchange_weak(old_head, new_head,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed));
  return true;
}

template <typename T>
bool BoundedQueue<T>::waitEnqueue(const T& element) {
  while (!break_all_wait_) {
    if (enqueue(element)) {
      return true;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock);
    continue;
  }

  return false;
}

template <typename T>
bool BoundedQueue<T>::waitEnqueue(T&& element) {
  while (!break_all_wait_) {
    if (enqueue(std::move(element))) {
      return true;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock);
    continue;
  }

  return false;
}

template <typename T>
bool BoundedQueue<T>::waitDequeue(T* element) {
  while (!break_all_wait_) {
    if (dequeue(element)) {
      return true;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock);
    continue;
  }

  return false;
}

template <typename T>
inline uint64_t BoundedQueue<T>::size() {
  return tail_ - head_ - 1;
}

template <typename T>
inline bool BoundedQueue<T>::empty() {
  return size() == 0;
}

template <typename T>
inline uint64_t BoundedQueue<T>::getIndex(uint64_t num) {
  return num - (num / pool_size_) * pool_size_;  // faster than %
}

template <typename T>
inline void BoundedQueue<T>::breakAllWait() {
  if (break_all_wait_.exchange(true)) {
    return;
  }
  cv_.notify_all();
}

}  // namespace utils
}  // namespace dm

