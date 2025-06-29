#include <thread>
#include <numeric>
#include <algorithm>
#include <vector>
#include <iostream>

// 使用函数对象可以获得更好的复用性
template <typename Iterator, typename T>
struct accumulate_block {
  void operator()(Iterator first, Iterator last, T& result) {
    result = std::accumulate(first, last, result);
  }
};

template <typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init) {
  unsigned long const length = std::distance(first, last);
  if (length == 0) {
    return init;
  }
  unsigned long const min_per_thread = 25;
  unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
  unsigned long const hardware_threads = std::thread::hardware_concurrency();
  unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
  unsigned long const block_size = length / num_threads;

  std::vector<T> results(num_threads);
  std::vector<std::thread> threads(num_threads - 1);
  Iterator block_start = first;
  for (unsigned long i = 0; i < (num_threads - 1); ++i) {
    Iterator block_end = block_start;
    std::advance(block_end, block_size);

    threads[i] = std::thread(
      accumulate_block<Iterator, T>(),
      block_start, block_end, std::ref(results[i])); // 使用std::ref传递结果
    block_start = block_end;
  }

  accumulate_block<Iterator, T>()(block_start, last, results[num_threads - 1]);
  for(auto& entry : threads) {
    entry.join();
  }
  return std::accumulate(results.begin(), results.end(), init);
}


int main() {
  std::vector<int> data(10000000, 1); // 1000万个元素，每个都是1
  
  auto start = std::chrono::high_resolution_clock::now();
  int std_result = std::accumulate(data.begin(), data.end(), 0);
  auto end = std::chrono::high_resolution_clock::now();
  auto std_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  std::cout << "std::accumulate result: " << std_result << std::endl;
  std::cout << "std::accumulate time: " << std_duration.count() << " microseconds" << std::endl;
  
  start = std::chrono::high_resolution_clock::now();
  int parallel_result = parallel_accumulate(data.begin(), data.end(), 0);
  end = std::chrono::high_resolution_clock::now();
  auto parallel_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  std::cout << "parallel_accumulate result: " << parallel_result << std::endl;
  std::cout << "parallel_accumulate time: " << parallel_duration.count() << " microseconds" << std::endl;
  
  double speedup = static_cast<double>(std_duration.count()) / parallel_duration.count();
  std::cout << "speedup: " << speedup << "x" << std::endl;
  
  if (std_result == parallel_result) {
    std::cout << "result validation: correct" << std::endl;
  } else {
    std::cout << "result validation: incorrect" << std::endl;
  }

  return 0;
}