#pragma once

#include <memory>

class function_wrapper {
  struct impl_base {
    virtual void call() = 0;
    virtual ~impl_base() {}
  };
  template <typename F>
  struct impl_type : impl_base {
    F f_;
    impl_type(F&& f) : f_(std::move(f)) {}
    void call() override { f_(); }
  };
  std::unique_ptr<impl_base> impl_;

 public:
  function_wrapper() = default;
  template <typename F>
  function_wrapper(F&& f) : impl_(new impl_type<F>(std::move(f))) {}
  function_wrapper(function_wrapper&& other) noexcept
      : impl_(std::move(other.impl_)) {}
  function_wrapper(const function_wrapper&) = delete;
  function_wrapper& operator=(const function_wrapper&) = delete;
  function_wrapper& operator=(function_wrapper&& other) noexcept {
    impl_ = std::move(other.impl_);
    return *this;
  }
  void operator()() { impl_->call(); }
};