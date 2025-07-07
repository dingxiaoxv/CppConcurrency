#include <atomic>
#include <memory>

template <typename T>
class lock_free_queue {
 private:
  struct node;
  struct counted_node_ptr {
    int external_count;
    node* ptr;
  } __attribute__((aligned(16)));

  struct node_counter {
    unsigned internal_count : 30;
    unsigned external_counters : 2;
  } __attribute__((packed));

  struct node {
    std::atomic<T*> data;
    std::atomic<node_counter> count;
    std::atomic<counted_node_ptr> next;
    node() {
      node_counter new_count;
      new_count.internal_count = 0;
      new_count.external_counters = 2;
      count.store(new_count);
      next.ptr = nullptr;
      next.external_count = 0;
    }

    void release_ref() {
      node_counter old_counter = count.load(std::memory_order_relaxed);
      node_counter new_counter;
      do {
        new_counter = old_counter;
        --new_counter.internal_count;
      } while (!count.compare_exchange_strong(old_counter, new_counter,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed));
      if (!new_counter.internal_count && !new_counter.external_counters) {
        delete this;
      }
    }
  };

  std::atomic<counted_node_ptr> head_;
  std::atomic<counted_node_ptr> tail_;

  void set_new_tail(counted_node_ptr& old_tail,
                    const counted_node_ptr& new_tail) {
    const node* current_tail_ptr = old_tail.ptr;
    while (!tail_.compare_exchange_weak(old_tail, new_tail) &&
           old_tail.ptr == current_tail_ptr) {
      if (old_tail.ptr == current_tail_ptr) {
        free_external_counter(old_tail);
      } else {
        current_tail_ptr->release_ref();
      }
    }
  }

  static void increase_external_count(std::atomic<counted_node_ptr>& counter,
                                      counted_node_ptr& old_counter) {
    counted_node_ptr new_counter;
    do {
      new_counter = old_counter;
      ++new_counter.external_count;
    } while (!counter.compare_exchange_strong(old_counter, new_counter,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed));
    old_counter.external_count = new_counter.external_count;
  }

  static void free_external_counter(counted_node_ptr& old_counter_ptr) {
    node* const ptr_to_delete = old_counter_ptr.ptr;
    int const count_increase = old_counter_ptr.external_count - 2;
    node_counter old_counter =
        ptr_to_delete->count.load(std::memory_order_relaxed);
    node_counter new_counter;
    do {
      new_counter = old_counter;
      --new_counter.external_counters;
      new_counter.internal_count += count_increase;
    } while (!ptr_to_delete->count.compare_exchange_strong(
        old_counter, new_counter, std::memory_order_acquire,
        std::memory_order_relaxed));
    if (!new_counter.internal_count && !new_counter.external_counters) {
      delete ptr_to_delete;
    }
  }

 public:
  void push(T new_value) {
    std::unique_ptr<T> new_data(new T(new_value));
    counted_node_ptr new_next;
    new_next.ptr = new node;
    new_next.external_count = 1;
    counted_node_ptr old_tail = tail_.load();
    for (;;) {
      increase_external_count(tail_, old_tail);
      T* old_data = nullptr;
      if (old_tail.ptr->data.compare_exchange_strong(old_data,
                                                     new_data.get())) {
        counted_node_ptr old_next = {0};
        if (!old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
          delete new_next.ptr;
          new_next = old_next;
        }
        set_new_tail(old_tail, new_next);
        new_data.release();
        break;
      } else {
        counted_node_ptr old_next = {0};
        if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
          old_next = new_next;
          new_next.ptr = new node;
        }
        set_new_tail(old_tail, new_next);
        new_data.release();
        break;
      }
    }
  }

  std::unique_ptr<T> pop() {
    counted_node_ptr old_head = head_.load(std::memory_order_relaxed);
    for (;;) {
      increase_external_count(head_, old_head);
      node* const old_head_ptr = old_head.ptr;
      if (old_head_ptr == tail_.load().ptr) {
        return std::unique_ptr<T>();
      }
      counted_node_ptr next =
          old_head_ptr->next.load(std::memory_order_relaxed);
      if (head_.compare_exchange_strong(old_head, next)) {
        T* const res = old_head_ptr->data.exchange(nullptr);
        free_external_counter(old_head);
        return std::unique_ptr<T>(res);
      }
      old_head_ptr->release_ref();
    }
  }
};