#include <future>
#include <thread>

class interrupt_flag {
 public:
  interrupt_flag() : flag_(false), thread_cv_(nullptr) {}
  void set() {
    flag_.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(set_clear_mutex_);
    if (thread_cv_) {
      thread_cv_->notify_all();
    }
  }

  bool is_set() const { return flag_.load(std::memory_order_relaxed); }

  void set_condition_variable(std::condition_variable* cv) {
    std::lock_guard<std::mutex> lk(set_clear_mutex_);
    thread_cv_ = cv;
  }

  void clear_condition_variable() {
    std::lock_guard<std::mutex> lk(set_clear_mutex_);
    thread_cv_ = nullptr;
  }

  struct clear_cv_on_destruct {
    ~clear_cv_on_destruct() {
      this_thread_interrupt_flag.clear_condition_variable();
    }
  };

  private:
  std::atomic<bool> flag_;
  std::condition_variable* thread_cv_;
  std::mutex set_clear_mutex_;
};

thread_local interrupt_flag this_thread_interrupt_flag;

class interruptible_thread {
 public:
  template <typename FunctionType>
  interruptible_thread(FunctionType f) {
    std::promise<interrupt_flag*> p;
    thread_ = std::thread([f, &p] {
      p.set_value(&this_thread_interrupt_flag);
      try {
        f();
      } catch (const thread_interrupted&) {
      }
    });
    interrupt_flag_ = p.get_future().get();
  }

  void interrupt() {
    if (interrupt_flag_) {
      interrupt_flag_->set();
    }
  }

  void interruption_point() {
    if (this_thread_interrupt_flag.is_set()) {
      throw thread_interrupted();
    }
  }

  void join() { thread_.join(); }

  void detach() { thread_.detach(); }

 private:
  void interruptible_wait(std::condition_variable& cv,
                          std::unique_lock<std::mutex>& lk) {
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(&cv);
    interrupt_flag::clear_cv_on_destruct guard;
    interruption_point();
    cv.wait_for(lk, std::chrono::milliseconds(1));
    interruption_point();
  }

 private:
  std::thread thread_;
  interrupt_flag* interrupt_flag_;
};