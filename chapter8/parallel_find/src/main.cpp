#include <algorithm>
#include <future>
#include <iostream>
#include <numeric>
#include <system_error>
#include <thread>
#include <vector>

class join_threads {
 public:
  explicit join_threads(std::vector<std::thread>& threads)
      : threads_(threads) {}
  ~join_threads() {
    for (unsigned long i = 0; i < threads_.size(); ++i) {
      if (threads_[i].joinable()) threads_[i].join();
    }
  }

 private:
  std::vector<std::thread>& threads_;
};

template <typename Iterator, typename MatchType>
struct find_element {
  void operator()(Iterator begin, Iterator end, MatchType match,
                  std::promise<Iterator>& result, std::atomic<bool>& done) {
    try {
      for (; (begin != end) && !done.load(); ++begin) {
        if (*begin == match) {
          // 即使多个线程同时找到匹配元素，promise只会接受第一个值
          // 后续的 set_value() 调用会抛出异常，被catch块处理
          result.set_value(begin);
          done.store(true);
          return;
        }
      }
    } catch (...) {
      try {
        result.set_exception(std::current_exception());
        done.store(true);
      } catch (...) {
      }
    }
  };
};

template <typename Iterator, typename MatchType>
Iterator parallel_find(Iterator first, Iterator last, MatchType match) {
  unsigned long const length = std::distance(first, last);
  if (!length) return last;
  unsigned long const min_per_thread = 25;
  unsigned long const max_threads =
      (length + min_per_thread - 1) / min_per_thread;
  unsigned long const hardware_threads = std::thread::hardware_concurrency();
  unsigned long const num_threads =
      std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
  unsigned long const block_size = length / num_threads;

  std::promise<Iterator> result;  // 使用std::promise实现多个线程返回一个结果
  std::atomic<bool> done(false);  // 使用std::atomic实现多线程控制
  std::vector<std::thread> threads(num_threads - 1);
  {
    join_threads joiner(threads);
    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); ++i) {
      Iterator block_end = block_start;
      std::advance(block_end, block_size);
      threads[i] =
          std::thread(find_element<Iterator, MatchType>(), block_start,
                      block_end, match, std::ref(result), std::ref(done));
    }
    find_element<Iterator, MatchType>()(block_start, last, match,
                                        std::ref(result), std::ref(done));
  }

  if (!done) {
    return last;
  }
  return result.get_future().get();
}

template <typename Iterator, typename MatchType>
Iterator parallel_find_async(Iterator first, Iterator last, MatchType match,
                            std::atomic<bool>& done) {
  try {
    unsigned long const length = std::distance(first, last);
    unsigned long const min_per_thread = 25;
    if (length < (2 * min_per_thread)) {
      for (; (first != last) && !done.load(); ++first) {
        if (*first == match) {
          done = true;
          return first;
        }
      }
      return last;
    } else {
      Iterator const mid_point = first + (length / 2);
      std::future<Iterator> async_result =
          std::async(&parallel_find_async<Iterator, MatchType>, mid_point, last,
                     match, std::ref(done));
      Iterator const direct_result =
          parallel_find_async(first, mid_point, match, done);
      return (direct_result == mid_point) ? async_result.get() : direct_result;
    }
  } catch (...) {
    done = true;
    throw;
  }
}

int main() {
  try {
    std::vector<int> data(10000000);         // 1000万个元素
    std::iota(data.begin(), data.end(), 1);  // 填充1到10000000

    // 测试查找中间位置的元素
    int target = 5000000;

    auto start = std::chrono::high_resolution_clock::now();
    auto std_result = std::find(data.begin(), data.end(), target);
    auto end = std::chrono::high_resolution_clock::now();
    auto std_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "std::find result: "
              << (std_result != data.end() ? *std_result : -1) << std::endl;
    std::cout << "std::find time: " << std_duration.count() << " microseconds"
              << std::endl;

    start = std::chrono::high_resolution_clock::now();
    auto parallel_result = parallel_find(data.begin(), data.end(), target);
    end = std::chrono::high_resolution_clock::now();
    auto parallel_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "parallel_find result: "
              << (parallel_result != data.end() ? *parallel_result : -1)
              << std::endl;
    std::cout << "parallel_find time: " << parallel_duration.count()
              << " microseconds" << std::endl;

    double speedup =
        static_cast<double>(std_duration.count()) / parallel_duration.count();
    std::cout << "speedup: " << speedup << "x" << std::endl;

    if (std_result == parallel_result) {
      std::cout << "result validation: correct" << std::endl;
    } else {
      std::cout << "result validation: incorrect" << std::endl;
    }

    // 测试查找不存在的元素
    std::cout << "\n--- Testing with non-existent element ---" << std::endl;
    target = 10000001;  // 不存在的元素

    start = std::chrono::high_resolution_clock::now();
    std_result = std::find(data.begin(), data.end(), target);
    end = std::chrono::high_resolution_clock::now();
    std_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "std::find time (not found): " << std_duration.count()
              << " microseconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    parallel_result = parallel_find(data.begin(), data.end(), target);
    end = std::chrono::high_resolution_clock::now();
    parallel_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "parallel_find time (not found): " << parallel_duration.count()
              << " microseconds" << std::endl;

    speedup =
        static_cast<double>(std_duration.count()) / parallel_duration.count();
    std::cout << "speedup (not found): " << speedup << "x" << std::endl;

  } catch (const std::system_error& e) {
    std::cerr << "thread create failed, " << e.what() << std::endl;
    return 1;
  } catch (const std::bad_alloc& e) {
    std::cerr << "badalloc, " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "exception, " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "unknown exception, " << std::endl;
    return 1;
  }

  return 0;
}
