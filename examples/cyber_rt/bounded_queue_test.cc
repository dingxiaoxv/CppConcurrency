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

#include "bounded_queue.h"

#include <thread>

#include "gtest/gtest.h"

namespace dm {
namespace utils {

TEST(BoundedQueueTest, enqueue) {
  BoundedQueue<int> queue;
  queue.init(100);
  EXPECT_EQ(0, queue.size());
  EXPECT_TRUE(queue.empty());
  for (int i = 1; i <= 100; i++) {
    EXPECT_TRUE(queue.enqueue(i));
    EXPECT_EQ(i, queue.size());
  }
  EXPECT_FALSE(queue.enqueue(101));
}

TEST(BoundedQueueTest, dequeue) {
  BoundedQueue<int> queue;
  queue.init(100);
  int value = 0;
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(queue.enqueue(i));
    EXPECT_TRUE(queue.dequeue(&value));
    EXPECT_EQ(i, value);
  }
  EXPECT_FALSE(queue.dequeue(&value));
}

TEST(BoundedQueueTest, concurrency) {
  BoundedQueue<int> queue;
  queue.init(10);
  std::atomic_int count = {0};
  std::thread threads[48];
  for (int i = 0; i < 48; ++i) {
    if (i % 4 == 0) {
      threads[i] = std::thread([&]() {
        for (int j = 0; j < 10000; ++j) {
          if (queue.enqueue(j)) {
            count++;
          }
        }
      });
    } else if (i % 4 == 1) {
      threads[i] = std::thread([&]() {
        for (int j = 0; j < 10000; ++j) {
          if (queue.waitEnqueue(j)) {
            count++;
          }
        }
      });
    } else if (i % 4 == 2) {
      threads[i] = std::thread([&]() {
        for (int j = 0; j < 10000; ++j) {
          int value = 0;
          if (queue.dequeue(&value)) {
            count--;
          }
        }
      });
    } else {
      threads[i] = std::thread([&]() {
        for (int j = 0; j < 10000; ++j) {
          int value = 0;
          if (queue.waitDequeue(&value)) {
            count--;
          }
        }
      });
    }
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  queue.breakAllWait();
  for (int i = 0; i < 48; ++i) {
    threads[i].join();
  }
  EXPECT_EQ(count.load(), queue.size());
}

TEST(BoundedQueueTest, waitDequeue) {
  BoundedQueue<int> queue;
  queue.init(100);
  queue.enqueue(10);
  std::thread t([&]() {
    int value = 0;
    queue.waitDequeue(&value);
    EXPECT_EQ(10, value);
    queue.waitDequeue(&value);
    EXPECT_EQ(100, value);
  });
  queue.enqueue(100);
  t.join();
}

}  // namespace utils
}  // namespace dm
