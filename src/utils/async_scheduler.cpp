#include "async_scheduler.h"

#include <spdlog/spdlog.h>

#include <algorithm>

async_scheduler::async_scheduler(std::shared_ptr<thread_pool> pool) : thread_pool_(std::move(pool)), running_(false), next_task_id_(1) { spdlog::debug("Async scheduler created"); }

async_scheduler::~async_scheduler() { stop(); }

void async_scheduler::start() {
  if (running_.exchange(true)) {
    return;  // Already running
  }

  spdlog::info("Starting async scheduler");
  scheduler_thread_ = std::thread([this] { scheduler_loop(); });
}

void async_scheduler::stop() {
  if (!running_.exchange(false)) {
    return;  // Already stopped
  }

  spdlog::info("Stopping async scheduler");
  condition_.notify_all();

  if (scheduler_thread_.joinable()) {
    scheduler_thread_.join();
  }

  spdlog::debug("Async scheduler stopped");
}

size_t async_scheduler::schedule_once(const std::chrono::seconds delay, std::function<void()> task) {
  const auto task_id = next_task_id_++;

  {
    const auto next_run = std::chrono::steady_clock::now() + delay;
    std::unique_lock<std::mutex> lock(queue_mutex_);
    task_queue_.emplace(scheduled_task{next_run, std::chrono::seconds(0), std::move(task), false, task_id});
  }

  condition_.notify_one();
  spdlog::trace("Scheduled one-time task {} to run in {}s", task_id, delay.count());
  return task_id;
}

size_t async_scheduler::schedule_repeating(const std::chrono::seconds interval, std::function<void()> task) {
  const auto task_id = next_task_id_++;
  {
    const auto next_run = std::chrono::steady_clock::now() + interval;
    std::unique_lock<std::mutex> lock(queue_mutex_);
    task_queue_.emplace(scheduled_task{next_run, interval, std::move(task), true, task_id});
  }

  condition_.notify_one();
  spdlog::trace("Scheduled repeating task {} with {}s interval", task_id, interval.count());
  return task_id;
}

bool async_scheduler::cancel_task(size_t task_id) {
  std::unique_lock<std::mutex> lock(queue_mutex_);

  // Create a new queue without the canceled task
  std::priority_queue<scheduled_task, std::vector<scheduled_task>, std::greater<>> new_queue;
  bool found = false;

  while (!task_queue_.empty()) {
    auto task = task_queue_.top();
    task_queue_.pop();

    if (task.id == task_id) {
      found = true;
      spdlog::trace("Canceled task {}", task_id);
    } else {
      new_queue.push(std::move(task));
    }
  }

  task_queue_ = std::move(new_queue);
  return found;
}

size_t async_scheduler::scheduled_count() const {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  return task_queue_.size();
}

void async_scheduler::scheduler_loop() {
  spdlog::debug("Scheduler loop started");

  while (running_) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (task_queue_.empty()) {
      // Wait for new tasks
      condition_.wait(lock, [this] { return !running_ || !task_queue_.empty(); });
      continue;
    }

    const auto now = std::chrono::steady_clock::now();
    auto next_task = task_queue_.top();

    if (next_task.next_run <= now) {
      // Time to execute this task
      task_queue_.pop();
      lock.unlock();

      // Submit task to thread pool with error recovery
      try {
        if (thread_pool_->is_stopping()) {
          spdlog::warn("Thread pool is stopping, skipping task {}", next_task.id);
          break;  // Exit scheduler loop if thread pool is stopping
        }

        thread_pool_->enqueue(next_task.task);
        spdlog::trace("Executed scheduled task {}", next_task.id);
      } catch (const std::exception& e) {
        spdlog::error("Failed to execute scheduled task {}: {}. Task will be retried on next cycle.", next_task.id, e.what());

        // For repeating tasks, attempt recovery by rescheduling
        if (next_task.repeating && running_) {
          next_task.next_run = now + std::chrono::seconds(10);  // Retry in 10 seconds
          lock.lock();
          task_queue_.push(std::move(next_task));
          lock.unlock();
          spdlog::info("Rescheduled failed task {} for retry in 10 seconds", next_task.id);
        }
      }

      // Re-schedule if it's a repeating task
      if (next_task.repeating && running_) {
        next_task.next_run = now + next_task.interval;
        lock.lock();
        task_queue_.push(std::move(next_task));
        lock.unlock();
      }
    } else {
      // Wait until next task is ready or new task arrives
      const auto wait_time = next_task.next_run - now;
      condition_.wait_for(lock, wait_time, [this, next_task] { return !running_ || (!task_queue_.empty() && task_queue_.top().next_run < next_task.next_run); });
    }
  }

  spdlog::debug("Scheduler loop ended");
}