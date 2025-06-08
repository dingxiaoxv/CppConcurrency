#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>


template <typename T>
class threadsafe_queue {
private:
  std::queue<T> data_;
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
    std::lock_guard<std::mutex> lock(mtx_);
    data_.push(value);
    cond_var_.notify_all(); // 通知所有等待的线程
  }

  void wait_and_pop(T& value) {
    std::unique_lock<std::mutex> lock(mtx_);
    cond_var_.wait(lock, [this] { return !data_.empty(); });
    value = data_.front();
    data_.pop();
  }
  
  std::shared_ptr<T> wait_and_pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    cond_var_.wait(lock, [this] { return !data_.empty(); });
    std::shared_ptr<T> res(std::make_shared<T>(data_.front()));
    data_.pop();
    return res;
  }
  
  bool try_pop(T& value) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (data_.empty()) {
      return false;
    }
    value = data_.front();
    data_.pop();
    return true;
  }

  std::shared_ptr<T> try_pop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (data_.empty()) {
      return std::shared_ptr<T>();
    }
    std::shared_ptr<T> res(std::make_shared<T>(data_.front()));
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
  
  auto push_func = [&q]() {
    for(int i = 0; i < 5; i++) {
      q.push(i);
      std::cout << "Thread " << std::this_thread::get_id() << " push: " << i << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  };

  auto pop_func = [&q]() {
    for(int i = 0; i < 5; i++) {
      int value;
      q.wait_and_pop(value);
      std::cout << "Thread " << std::this_thread::get_id() << " pop: " << value << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  };

  std::thread t1(push_func);
  std::thread t2(pop_func);
  std::thread t3(pop_func);

  t1.join();
  t2.join();
  t3.join();

  return 0;
}