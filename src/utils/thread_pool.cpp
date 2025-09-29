#include "thread_pool.h"

#include <spdlog/spdlog.h>

thread_pool::thread_pool(const size_t num_threads) : stop_(false) {
  // Use at least 2 threads, but cap at 32 for reasonable resource usage
  size_t actual_threads = std::max(2UL, std::min(num_threads, 32UL));

  spdlog::info("Creating thread pool with {} threads", actual_threads);

  for (size_t i = 0; i < actual_threads; ++i) {
    workers_.emplace_back([this, i] {
      spdlog::debug("Thread pool worker {} started", i);

      while (true) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(queue_mutex_);
          condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

          if (stop_ && tasks_.empty()) {
            spdlog::debug("Thread pool worker {} exiting", i);
            return;
          }

          task = std::move(tasks_.front());
          tasks_.pop();
        }

        try {
          task();
        } catch (const std::exception& e) {
          spdlog::error("Thread pool worker {} caught exception: {}", i, e.what());
        } catch (...) {
          spdlog::error("Thread pool worker {} caught unknown exception", i);
        }
      }
    });
  }
}

thread_pool::~thread_pool() {
  spdlog::debug("Shutting down thread pool with {} threads", workers_.size());

  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }

  condition_.notify_all();

  for (std::thread& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  spdlog::info("Thread pool shutdown complete");
}

size_t thread_pool::pending_tasks() const {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  return tasks_.size();
}