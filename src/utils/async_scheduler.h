#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "task_recovery_policy.h"
#include "thread_pool.h"

// Task scheduled for future execution
struct scheduled_task {
  std::chrono::steady_clock::time_point next_run;
  std::chrono::seconds interval;
  std::function<void()> task;
  bool repeating;
  size_t id;
  int failure_count = 0;

  // Comparison for priority queue (earlier times have higher priority)
  bool operator>(const scheduled_task& other) const { return next_run > other.next_run; }
};

class async_scheduler {
 public:
  explicit async_scheduler(std::shared_ptr<thread_pool> pool, std::unique_ptr<task_recovery_policy> recovery_policy = std::make_unique<fixed_delay_recovery>());
  ~async_scheduler();

  // Schedule a one-time task
  size_t schedule_once(std::chrono::seconds delay, std::function<void()> task);

  // Schedule a repeating task
  size_t schedule_repeating(std::chrono::seconds interval, std::function<void()> task);

  // Cancel a scheduled task
  bool cancel_task(size_t task_id);

  // Get number of scheduled tasks
  [[nodiscard]] size_t scheduled_count() const;

  // Start the scheduler
  void start();

  // Stop the scheduler
  void stop();

 private:
  std::shared_ptr<thread_pool> thread_pool_;
  std::unique_ptr<task_recovery_policy> recovery_policy_;
  std::priority_queue<scheduled_task, std::vector<scheduled_task>, std::greater<>> task_queue_;
  mutable std::mutex queue_mutex_;
  std::condition_variable condition_;
  std::thread scheduler_thread_;
  std::atomic<bool> running_;
  std::atomic<size_t> next_task_id_;

  void scheduler_loop();
};