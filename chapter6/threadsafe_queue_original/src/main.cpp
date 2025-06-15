#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>


template <typename T>
class threadsafe_queue {
private:
  std::queue<std::shared_ptr<T>> data_;
  mutable std::mutex mtx_; // 确保const对象可以使用
  std::condition_variable cond_var_;

public:
  threadsafe_queue() {}
  threadsafe_queue(const threadsafe_queue& other) {
    std::lock_guard<std::mutex> lock(other.mtx_);
    data_ = other.data_;
  }
  threadsafe_queue& operator=(const threadsafe_queue&) = delete;

  void push(T value) {
    std::shared_ptr<T> new_value(std::make_shared<T>(std::move(value))); //创建对象的操作不必放到锁内, 提高性能
    std::lock_guard<std::mutex> lock(mtx_);
    data_.push(new_value);
    cond_var_.notify_one();
  }

  void wait_and_pop(T& value) {
    std::unique_lock<std::mutex> lock(mtx_);
    cond_var_.wait(lock, [this] { return !data_.empty(); });
    value = *data_.front();
    data_.pop();
  }
  
  std::shared_ptr<T> wait_and_pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    cond_var_.wait(lock, [this] { return !data_.empty(); });
    std::shared_ptr<T> res = data_.front(); // 安全的拷贝操作
    data_.pop();
    return res;
  }
  
  bool try_pop(T& value) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (data_.empty()) {
      return false;
    }
    value = *data_.front();
    data_.pop();
    return true;
  }

  std::shared_ptr<T> try_pop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (data_.empty()) {
      return std::shared_ptr<T>();
    }
    std::shared_ptr<T> res = data_.front();
    data_.pop();
    return res;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return data_.empty();
  }
};

int main() {
  threadsafe_queue<int> q;
  
  std::vector<std::thread> threads;
  
  threads.emplace_back([&q]() {
    for(int i = 0; i < 5; i++) {
      q.push(i);
      std::cout << "Thread " << std::this_thread::get_id() << " push: " << i << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  
  threads.emplace_back([&q]() {
    for(int i = 5; i < 10; i++) {
      q.push(i);
      std::cout << "Thread " << std::this_thread::get_id() << " push: " << i << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  
  threads.emplace_back([&q]() {
    for(int i = 0; i < 10; i++) {
      int value = *q.wait_and_pop();
      std::cout << "Thread " << std::this_thread::get_id() << " pop: " << value << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  
  for(auto& t : threads) {
    t.join();
  }
  
  return 0;
}