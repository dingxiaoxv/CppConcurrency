// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/log_msg_buffer.h>
#include <spdlog/details/mpmc_blocking_q.h>
#include <spdlog/details/os.h>

#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace spdlog {
class async_logger;

namespace details {

using async_logger_ptr = std::shared_ptr<spdlog::async_logger>;

enum class async_msg_type { log, flush, terminate };

// Async msg to move to/from the queue
// Movable only. should never be copied
struct async_msg : log_msg_buffer {
    async_msg_type msg_type{async_msg_type::log};
    async_logger_ptr worker_ptr;

    async_msg() = default;
    ~async_msg() = default;

    // should only be moved in or out of the queue..
    async_msg(const async_msg &) = delete;

// support for vs2013 move
#if defined(_MSC_VER) && _MSC_VER <= 1800
    async_msg(async_msg &&other)
        : log_msg_buffer(std::move(other)),
          msg_type(other.msg_type),
          worker_ptr(std::move(other.worker_ptr)) {}

    async_msg &operator=(async_msg &&other) {
        *static_cast<log_msg_buffer *>(this) = std::move(other);
        msg_type = other.msg_type;
        worker_ptr = std::move(other.worker_ptr);
        return *this;
    }
#else  // (_MSC_VER) && _MSC_VER <= 1800
    async_msg(async_msg &&) = default;
    async_msg &operator=(async_msg &&) = default;
#endif

    // construct from log_msg with given type
    async_msg(async_logger_ptr &&worker, async_msg_type the_type, const details::log_msg &m)
        : log_msg_buffer{m},
          msg_type{the_type},
          worker_ptr{std::move(worker)} {}

    async_msg(async_logger_ptr &&worker, async_msg_type the_type)
        : log_msg_buffer{},
          msg_type{the_type},
          worker_ptr{std::move(worker)} {}

    explicit async_msg(async_msg_type the_type)
        : async_msg{nullptr, the_type} {}
};

class SPDLOG_API thread_pool {
public:
    using item_type = async_msg;
    using q_type = details::mpmc_blocking_queue<item_type>;

    thread_pool(size_t q_max_items,
                size_t threads_n,
                std::function<void()> on_thread_start,
                std::function<void()> on_thread_stop);
    thread_pool(size_t q_max_items, size_t threads_n, std::function<void()> on_thread_start);
    thread_pool(size_t q_max_items, size_t threads_n);

    // message all threads to terminate gracefully and join them
    ~thread_pool();

    thread_pool(const thread_pool &) = delete;
    thread_pool &operator=(thread_pool &&) = delete;

    void post_log(async_logger_ptr &&worker_ptr,
                  const details::log_msg &msg,
                  async_overflow_policy overflow_policy);
    void post_flush(async_logger_ptr &&worker_ptr, async_overflow_policy overflow_policy);
    size_t overrun_counter();
    void reset_overrun_counter();
    size_t discard_counter();
    void reset_discard_counter();
    size_t queue_size();

private:
    q_type q_;

    std::vector<std::thread> threads_;

    void post_async_msg_(async_msg &&new_msg, async_overflow_policy overflow_policy);
    void worker_loop_();

    // process next message in the queue
    // return true if this thread should still be active (while no terminate msg
    // was received)
    bool process_next_msg_();
};

SPDLOG_INLINE thread_pool::thread_pool(size_t q_max_items,
                                       size_t threads_n,
                                       std::function<void()> on_thread_start,
                                       std::function<void()> on_thread_stop)
    : q_(q_max_items) {
    if (threads_n == 0 || threads_n > 1000) {
        throw_spdlog_ex(
            "spdlog::thread_pool(): invalid threads_n param (valid "
            "range is 1-1000)");
    }
    for (size_t i = 0; i < threads_n; i++) {
        threads_.emplace_back([this, on_thread_start, on_thread_stop] {
            on_thread_start();
            this->thread_pool::worker_loop_();
            on_thread_stop();
        });
    }
}

SPDLOG_INLINE thread_pool::thread_pool(size_t q_max_items,
                                       size_t threads_n,
                                       std::function<void()> on_thread_start)
    : thread_pool(q_max_items, threads_n, std::move(on_thread_start), [] {}) {}

SPDLOG_INLINE thread_pool::thread_pool(size_t q_max_items, size_t threads_n)
    : thread_pool(q_max_items, threads_n, [] {}, [] {}) {}

// message all threads to terminate gracefully join them
SPDLOG_INLINE thread_pool::~thread_pool() {
    SPDLOG_TRY {
        for (size_t i = 0; i < threads_.size(); i++) {
            post_async_msg_(async_msg(async_msg_type::terminate), async_overflow_policy::block);
        }

        for (auto &t : threads_) {
            t.join();
        }
    }
    SPDLOG_CATCH_STD
}

void SPDLOG_INLINE thread_pool::post_log(async_logger_ptr &&worker_ptr,
                                         const details::log_msg &msg,
                                         async_overflow_policy overflow_policy) {
    async_msg async_m(std::move(worker_ptr), async_msg_type::log, msg);
    post_async_msg_(std::move(async_m), overflow_policy);
}

void SPDLOG_INLINE thread_pool::post_flush(async_logger_ptr &&worker_ptr,
                                           async_overflow_policy overflow_policy) {
    post_async_msg_(async_msg(std::move(worker_ptr), async_msg_type::flush), overflow_policy);
}

size_t SPDLOG_INLINE thread_pool::overrun_counter() { return q_.overrun_counter(); }

void SPDLOG_INLINE thread_pool::reset_overrun_counter() { q_.reset_overrun_counter(); }

size_t SPDLOG_INLINE thread_pool::discard_counter() { return q_.discard_counter(); }

void SPDLOG_INLINE thread_pool::reset_discard_counter() { q_.reset_discard_counter(); }

size_t SPDLOG_INLINE thread_pool::queue_size() { return q_.size(); }

void SPDLOG_INLINE thread_pool::post_async_msg_(async_msg &&new_msg,
                                                async_overflow_policy overflow_policy) {
    if (overflow_policy == async_overflow_policy::block) {
        q_.enqueue(std::move(new_msg));
    } else if (overflow_policy == async_overflow_policy::overrun_oldest) {
        q_.enqueue_nowait(std::move(new_msg));
    } else {
        assert(overflow_policy == async_overflow_policy::discard_new);
        q_.enqueue_if_have_room(std::move(new_msg));
    }
}

void SPDLOG_INLINE thread_pool::worker_loop_() {
    while (process_next_msg_()) {
    }
}

// process next message in the queue
// returns true if this thread should still be active (while no terminated msg was received)
bool SPDLOG_INLINE thread_pool::process_next_msg_() {
    async_msg incoming_async_msg;
    q_.dequeue(incoming_async_msg);

    switch (incoming_async_msg.msg_type) {
        case async_msg_type::log: {
            incoming_async_msg.worker_ptr->backend_sink_it_(incoming_async_msg);
            return true;
        }
        case async_msg_type::flush: {
            incoming_async_msg.worker_ptr->backend_flush_();
            return true;
        }

        case async_msg_type::terminate: {
            return false;
        }

        default: {
            assert(false);
        }
    }

    return true;
}

}  // namespace details
}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "thread_pool-inl.h"
#endif
