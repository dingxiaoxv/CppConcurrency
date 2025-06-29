#include <stack>
#include <mutex>
#include <memory>
#include <exception>

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