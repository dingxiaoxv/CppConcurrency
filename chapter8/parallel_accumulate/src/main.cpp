#include <algorithm>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <numeric>
#include <system_error>
#include <thread>
#include <vector>

// 使用RAII来管理线程的join
class join_threads {
 public:
  explicit join_threads(std::vector<std::thread>& threads_)
      : threads(threads_) {}
  ~join_threads() {
    for (unsigned long i = 0; i < threads.size(); ++i) {
      if (threads[i].joinable()) threads[i].join();
    }
  }

 private:
  std::vector<std::thread>& threads;
};

// 使用函数对象可以获得更好的复用性
template <typename Iterator, typename T>
struct accumulate_block {
  T operator()(Iterator first, Iterator last) {
    return std::accumulate(first, last, T());
  }
};

template <typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init) {
  unsigned long const length = std::distance(first, last);
  if (length == 0) {
    return init;
  }
  unsigned long const min_per_thread = 25;
  unsigned long const max_threads =
      (length + min_per_thread - 1) / min_per_thread;
  unsigned long const hardware_threads = std::thread::hardware_concurrency();
  unsigned long const num_threads =
      std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
  unsigned long const block_size = length / num_threads;

  std::vector<std::future<T>> futures(num_threads - 1);
  std::vector<std::thread> threads(num_threads - 1);
  join_threads joiner(threads);

  Iterator block_start = first;
  for (unsigned long i = 0; i < (num_threads - 1); ++i) {
    Iterator block_end = block_start;
    std::advance(block_end, block_size);
    // std::packaged_task可以配合std::future来获取线程的返回值 且是线程安全的
    std::packaged_task<T(Iterator, Iterator)> task{
        accumulate_block<Iterator, T>()};
    futures[i] = task.get_future();
    // 如果某个线程创建失败 joiner会自动join所有线程确保不会发生线程泄露
    threads[i] = std::thread(std::move(task), block_start, block_end);
    block_start = block_end;
  }
  T last_result = accumulate_block<Iterator, T>()(block_start, last);

  T result = init;
  for (unsigned long i = 0; i < (num_threads - 1); ++i) {
    result += futures[i].get();
  }
  result += last_result;

  return result;
}  // join_threads RAII对象会在函数结束时自动join所有线程，无需手动join

int main() {
  try {
    std::vector<int> data(10000000, 1);  // 1000万个元素，每个都是1

    auto start = std::chrono::high_resolution_clock::now();
    int std_result = std::accumulate(data.begin(), data.end(), 0);
    auto end = std::chrono::high_resolution_clock::now();
    auto std_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "std::accumulate result: " << std_result << std::endl;
    std::cout << "std::accumulate time: " << std_duration.count()
              << " microseconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    int parallel_result = parallel_accumulate(data.begin(), data.end(), 0);
    end = std::chrono::high_resolution_clock::now();
    auto parallel_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "parallel_accumulate result: " << parallel_result << std::endl;
    std::cout << "parallel_accumulate time: " << parallel_duration.count()
              << " microseconds" << std::endl;

    double speedup =
        static_cast<double>(std_duration.count()) / parallel_duration.count();
    std::cout << "speedup: " << speedup << "x" << std::endl;

    if (std_result == parallel_result) {
      std::cout << "result validation: correct" << std::endl;
    } else {
      std::cout << "result validation: incorrect" << std::endl;
    }

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