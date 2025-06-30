#include <condition_variable>
#include <memory>
#include <mutex>

template <typename T>
class threadsafe_queue {
  struct node {
    std::shared_ptr<T> data;
    std::unique_ptr<node> next;
  };

 public:
  threadsafe_queue() : head_(new node), tail_(head_.get()) {}
  threadsafe_queue(const threadsafe_queue& other) = delete;
  threadsafe_queue& operator=(const threadsafe_queue& other) = delete;
  void push(T new_value) {
    std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
    std::unique_ptr<node> p(new node);
    {
      std::lock_guard<std::mutex> lock(tail_mutex_);
      tail_->data = new_data;
      node* const new_tail = p.get();
      tail_->next = std::move(p);
      tail_ = new_tail;
    }
    cond_.notify_one();
  }
  std::shared_ptr<T> try_pop() {
    std::unique_ptr<node> old_head = try_pop_head();
    return old_head ? old_head->data : std::shared_ptr<T>();
  }
  bool try_pop(T& value) {
    std::unique_ptr<node> const old_head = try_pop_head();
    if (old_head) {
      value = std::move(*old_head->data);
      return true;
    }
    return false;
  }
  std::shared_ptr<T> wait_and_pop() {
    std::unique_ptr<node> const old_head = wait_pop_head();
    return old_head->data;
  }
  void wait_and_pop(T& value) {
    std::unique_ptr<node> const old_head = wait_pop_head();
    value = std::move(*old_head->data);
  }
  bool empty() {
    std::lock_guard<std::mutex> lock(head_mutex_);
    return head_.get() == get_tail();
  }

 private:
  node* get_tail() {
    std::lock_guard<std::mutex> lock(tail_mutex_);
    return tail_;
  }
  std::unique_ptr<node> pop_head() {
    std::unique_ptr<node> old_head = std::move(head_);
    head_ = std::move(old_head->next);
    return old_head;
  }
  std::unique_ptr<node> try_pop_head() {
    std::lock_guard<std::mutex> lock(head_mutex_);
    if (head_.get() == get_tail()) {
      return nullptr;
    }
    return pop_head();
  }
  std::unique_ptr<node> wait_pop_head() {
    std::unique_lock<std::mutex> lock(head_mutex_);
    cond_.wait(lock, [&] { return head_.get() != get_tail(); });
    return pop_head();
  }

 private:
  mutable std::mutex head_mutex_;
  mutable std::mutex tail_mutex_;
  std::condition_variable cond_;
  std::unique_ptr<node> head_;
  node* tail_;  // 指向"虚位"节点(dummy node)
};