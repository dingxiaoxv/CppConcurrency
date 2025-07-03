
#include <atomic>
#include <future>
#include <thread>
#include <vector>

#include "function_wrapper.h"
#include "join_threads.h"
#include "threadsafe_queue.h"

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
  std::future<typename std::invoke_result<FunctionType>::type> submit(
      FunctionType f) {
    using result_type = typename std::invoke_result<FunctionType>::type;
    std::packaged_task<result_type()> task(std::move(f));
    std::future<result_type> result(task.get_future());

    if (done_) {
      return std::future<result_type>();
    }
    work_queue_.push(std::move(task));
    return result;
  }

  void run_pending_task() {
    function_wrapper task;
    if (work_queue_.try_pop(task)) {
      task();
    } else {
      std::this_thread::yield();
    }
  }

 private:
  void worker_thread() {
    while (!done_) {
      function_wrapper task;
      if (work_queue_.try_pop(task)) {
        task();
      } else {
        std::this_thread::yield();
      }
    }
  }

 private:
  std::atomic<bool> done_;
  threadsafe_queue<function_wrapper> work_queue_;
  std::vector<std::thread> threads_;
  join_threads joiner_;
};