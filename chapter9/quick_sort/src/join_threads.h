#pragma once

#include <thread>
#include <vector>

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