#include <algorithm>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <system_error>
#include <thread>
#include <vector>

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

template <typename Iterator, typename Function>
void parallel_for_each(Iterator first, Iterator last, Function f) {
  unsigned long const length = std::distance(first, last);

  if (length == 0) {
    return;
  }

  unsigned long const min_per_thread = 25;
  unsigned long const max_threads =
      (length + min_per_thread - 1) / min_per_thread;
  unsigned long const hardware_threads = std::thread::hardware_concurrency();
  unsigned long const num_threads =
      std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
  unsigned long const block_size = length / num_threads;

  std::vector<std::future<void>> futures(num_threads - 1);
  std::vector<std::thread> threads(num_threads - 1);
  join_threads joiner(threads);

  Iterator block_start = first;
  for (unsigned long i = 0; i < (num_threads - 1); ++i) {
    Iterator block_end = block_start;
    std::advance(block_end, block_size);
    std::packaged_task<void(void)> task{
        [=]() { std::for_each(block_start, block_end, f); }};
    futures[i] = task.get_future();
    threads[i] = std::thread(std::move(task));
    block_start = block_end;
  }
  std::for_each(block_start, last, f);

  for (unsigned long i = 0; i < (num_threads - 1); ++i) {
    futures[i].get();  // 检查工作线程是否有异常
  }
}

template <typename Iterator, typename Func>
void parallel_for_each_async(Iterator first, Iterator last, Func f) {
  unsigned long const length = std::distance(first, last);
  if (!length) return;
  unsigned long const min_per_thread = 25;
  if (length < (2 * min_per_thread)) {
    std::for_each(first, last, f);
  } else {
    Iterator const mid_point = first + length / 2;
    std::future<void> first_half =
        std::async([=]() { parallel_for_each(first, mid_point, f); });
    parallel_for_each(mid_point, last, f);
    first_half.get();
  }
}

int main() {
  try {
    std::vector<int> data(10000000, 1);  // 1000万个元素，每个都是1

    auto start = std::chrono::high_resolution_clock::now();
    std::for_each(data.begin(), data.end(), [](int& value) { value *= 2; });
    auto end = std::chrono::high_resolution_clock::now();
    auto std_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "std::for_each time: " << std_duration.count()
              << " microseconds" << std::endl;

    std::fill(data.begin(), data.end(), 1);

    start = std::chrono::high_resolution_clock::now();
    parallel_for_each(data.begin(), data.end(), [](int& value) { value *= 2; });
    end = std::chrono::high_resolution_clock::now();
    auto parallel_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "parallel_for_each time: " << parallel_duration.count()
              << " microseconds" << std::endl;

    double speedup =
        static_cast<double>(std_duration.count()) / parallel_duration.count();
    std::cout << "speedup: " << speedup << "x" << std::endl;

    bool all_correct = std::all_of(data.begin(), data.end(),
                                   [](int value) { return value == 2; });
    if (all_correct) {
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