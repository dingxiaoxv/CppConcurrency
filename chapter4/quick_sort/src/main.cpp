#include <algorithm>
#include <future>
#include <iostream>
#include <list>

template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
  if (input.empty()) {
    return input;
  }
  std::list<T> result;
  result.splice(result.begin(), input, input.begin());
  T const& pivot = *result.begin();
  auto divide_point = std::partition(input.begin(), input.end(),
                                     [&](T const& t) { return t < pivot; });
  std::list<T> lower_part;
  lower_part.splice(lower_part.end(), input, input.begin(), divide_point);
  std::future<std::list<T> > new_lower(
      std::async(&parallel_quick_sort<T>, std::move(lower_part)));
  auto new_higher(parallel_quick_sort(std::move(input)));
  result.splice(result.end(), new_higher);
  result.splice(result.begin(), new_lower.get());
  return result;
}

int main() {
  std::list<int> l{7, 3, 9, 1, 4, 2, 8, 5, 10, 6};
  auto sorted = parallel_quick_sort(l);
  for (auto& i : sorted) {
    std::cout << i << " ";
  }
  std::cout << std::endl;
}