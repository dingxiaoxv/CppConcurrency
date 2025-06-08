#include <stack>
#include <mutex>
#include <memory>
#include <exception>
#include <thread>
#include <iostream>

struct empty_stack : std::exception {
  const char* what() const throw() {
    return "empty stack";
  }
};

template <typename T>
class threadsafe_stack {
  private:
    std::stack<T> data_;
    mutable std::mutex mtx_; // 确保const对象可以使用

  public:
    threadsafe_stack() {}
    threadsafe_stack(const threadsafe_stack& other) {
      std::lock_guard<std::mutex> lock(other.mtx_);
      data_ = other.data_;
    }
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(T value) {
      std::lock_guard<std::mutex> lock(mtx_);
      data_.push(value);
    }

    std::shared_ptr<T> pop() {
      std::lock_guard<std::mutex> lock(mtx_);
      if (data_.empty()) {
        throw empty_stack();
      }
      std::shared_ptr<T> const res(std::make_shared<T>(data_.top()));
      data_.pop();
      return res;
    }

    void pop(T& value) {
      std::lock_guard<std::mutex> lock(mtx_);
      if (data_.empty()) {
        return;
      }
      value = data_.top();
      data_.pop();
    }

    bool empty() const {
      std::lock_guard<std::mutex> lock(mtx_);
      return data_.empty();
    }
};


int main() {
  threadsafe_stack<int> stack;
  
  auto push_func = [&stack]() {
    for(int i = 0; i < 5; i++) {
      stack.push(i);
      std::cout << "Thread " << std::this_thread::get_id() << " Pushed value: " << i << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  };

  auto pop_func = [&stack]() {
    for(int i = 0; i < 5; i++) {
      try {
        std::shared_ptr<int> value = stack.pop();
        std::cout << "Thread " << std::this_thread::get_id() << " Popped value: " << *value << std::endl;
      } catch(const empty_stack& e) {
        std::cout << "Thread " << std::this_thread::get_id() << " " << e.what() << std::endl;
      }
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