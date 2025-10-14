#include "monitors.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <utility>

#include "../testers/network_test_ping.h"
#include "monitor_config.h"

monitors::monitors(const monitor_config& config, std::shared_ptr<push_notification_manager> push_manager)
    : config_(config), running_(false), push_manager_(std::move(std::move(push_manager))) {
  // Using auto-fallback ping implementation
  spdlog::info("Using auto-fallback ping implementation");

  // Create thread pool with configurable size
  const auto& monitor_groups = config.get_monitors();
  const size_t num_monitors = std::accumulate(monitor_groups.begin(), monitor_groups.end(), 0UL, [](const size_t sum, const group& g) { return sum + g.get_destination_count(); });

  size_t pool_size;
  if (config.get_thread_pool_size() > 0) {
    // Use configured thread pool size
    pool_size = config.get_thread_pool_size();
    spdlog::info("Using configured thread pool size: {}", pool_size);
  } else {
    // Calculate optimal thread pool size: balance between concurrency and resource usage
    const size_t hardware_threads = std::thread::hardware_concurrency();
    pool_size = std::min({
        std::max(4UL, hardware_threads),  // At least 4 threads
        num_monitors / 4 + 1,             // 1 thread per 4 monitors
        24UL                              // Cap at 24 threads
    });
    spdlog::info("Using auto-calculated thread pool size: {} (hardware: {}, monitors: {})", pool_size, hardware_threads, num_monitors);
  }

  thread_pool_ = std::make_shared<thread_pool>(pool_size);
  scheduler_ = std::make_unique<async_scheduler>(thread_pool_);

  spdlog::info("Created thread pool with {} threads for {} monitors", thread_pool_->thread_count(), num_monitors);

  int total_monitors = 0;

  for (const auto& group : config.get_monitors()) {
    std::string group_name = group.get_group_name();  // Store to avoid warning
    spdlog::info("Initializing monitor group: {}", group_name);

    for (const auto& dest : group.get_destinations()) {
      try {
        const std::string key = group.get_group_name() + ":" + dest.get_name();
        const auto state = std::make_shared<monitor_state>(dest, group);
        monitors_map_[key] = state;
        ++total_monitors;

        std::string test_desc = state->get_test_description();  // Store to avoid warning
        spdlog::debug("Initialized monitor: {} ({})", dest.get_name(), test_desc);
      } catch (const std::exception& e) {
        spdlog::error("Failed to initialize monitor {}: {}", dest.get_name(), e.what());
        throw std::runtime_error("Invalid test configuration for " + dest.get_name());
      }
    }
  }

  spdlog::info("Initialized {} monitors across {} groups", total_monitors, config.get_monitor_count());
}

monitors::~monitors() { stop_monitoring(); }

void monitors::start_monitoring() {
  if (running_.exchange(true)) {
    return;  // Already running
  }

  spdlog::info("Starting async monitoring tasks");

  // Start the scheduler
  scheduler_->start();

  // Schedule all monitors
  for (const auto& [key, state] : monitors_map_) {
    schedule_monitor_test(state);
    spdlog::debug("Scheduled async monitor: {} (interval: {}s)", key, state->get_destination().get_interval());
  }

  spdlog::info("All {} monitoring tasks scheduled with {} threads", monitors_map_.size(), thread_pool_->thread_count());
}

void monitors::stop_monitoring() {
  if (!running_.exchange(false)) {
    return;  // Already stopped
  }

  spdlog::info("Stopping async monitoring tasks");

  // Cancel all scheduled tasks
  for (const auto task_id : scheduled_task_ids_) {
    scheduler_->cancel_task(task_id);
  }
  scheduled_task_ids_.clear();

  // Stop the scheduler
  scheduler_->stop();

  spdlog::info("All monitoring tasks stopped");
}

void monitors::schedule_monitor_test(const std::shared_ptr<monitor_state>& state) {
  const auto interval = std::chrono::seconds(state->get_destination().get_interval());

  // Schedule the repeating task
  const auto task_id = scheduler_->schedule_repeating(interval, [this, state]() {
    if (running_) {
      perform_test_async(state);
    }
  });

  scheduled_task_ids_.push_back(task_id);
}

void monitors::perform_test_async(const std::shared_ptr<monitor_state>& state) const {
  if (!running_) {
    spdlog::debug("Monitoring stopped, skipping test for {}", state->get_destination().get_name());
    return;
  }

  try {
    // Execute test asynchronously
    auto future = execute_test_async(state);

    // Handle result asynchronously to avoid blocking the scheduler
    thread_pool_->enqueue([this, state, future = std::move(future)]() mutable {
      try {
        // Add timeout protection for future.get()
        const auto timeout = std::chrono::milliseconds(state->get_destination().get_timeout() + 5000);  // 5s buffer
        const auto status = future.wait_for(timeout);

        test_result result{false, 0, std::chrono::system_clock::now(), "Unknown error"};

        if (std::future_status::ready == status) {
          try {
            result = future.get();
          } catch (const std::exception& e) {
            spdlog::debug("Test execution failed for {}: {}", state->get_destination().get_name(), e.what());
            result = test_result{false, static_cast<long>(timeout.count()), std::chrono::system_clock::now(), e.what()};
          }
        } else if (std::future_status::timeout == status) {
          spdlog::warn("Test timeout exceeded for {} ({}ms + 5s buffer)", state->get_destination().get_name(), state->get_destination().get_timeout());
          result = test_result{false, static_cast<long>(timeout.count()), std::chrono::system_clock::now(), "Test timeout exceeded"};
        } else {
          spdlog::error("Test deferred/canceled for {}", state->get_destination().get_name());
          result = test_result{false, 0, std::chrono::system_clock::now(), "Test deferred or canceled"};
        }

        // Track previous status before adding result
        const monitor_status prev_status = state->get_current_status();

        state->add_result(result);

        // Detect status changes and send notifications
        const monitor_status new_status = state->get_current_status();

        // Send notification on ANY status change
        if (push_manager_ && prev_status != new_status) {
          std::string icon_emoji;
          std::string notification_body;

          // Determine icon and message based on new status
          switch (new_status) {
            case monitor_status::ok:
              icon_emoji = "✅";
              notification_body = "Monitor recovered to OK";
              spdlog::info("Monitor {} recovered to OK status", state->get_destination().get_name());
              break;
            case monitor_status::warning:
              icon_emoji = "⚠️";
              notification_body = "Monitor entered WARNING state";
              spdlog::warn("Monitor {} status: WARNING (consecutive failures: {})",
                          state->get_destination().get_name(), state->get_consecutive_failures());
              break;
            case monitor_status::failure:
              icon_emoji = "❌";
              notification_body = "Monitor entered FAILURE state";
              spdlog::warn("Monitor {} status: FAILURE (consecutive failures: {})",
                          state->get_destination().get_name(), state->get_consecutive_failures());
              break;
            case monitor_status::pending:
              icon_emoji = "⏳";
              notification_body = "Monitor is PENDING";
              break;
          }

          const std::string title = icon_emoji + " " + state->get_destination().get_name() + " - " + to_string(new_status);
          const std::string test_id = state->get_unique_id();
          spdlog::info("Triggering push notification for status change {} -> {}: {} (test_id: {})",
                      to_string(prev_status), to_string(new_status), title, test_id);
          push_manager_->send_notification_for_test(test_id, title, notification_body, "./icons/icon-192x192.png");
        } else if (!result.is_success() && monitor_status::ok != new_status) {
          // Log failures even without status change (with throttling)
          const int consecutive_failures = state->get_consecutive_failures();
          const int log_every_n = config_.get_log_status_every_n();

          // Always log first failure (consecutive_failures == 1)
          // Or log every N times if configured (e.g., 1, 50, 100, 150, ...)
          // Or always log if log_every_n == 0 (disabled throttling)
          bool should_log = (consecutive_failures == 1) ||
                           (log_every_n > 0 && consecutive_failures % log_every_n == 0) ||
                           (log_every_n == 0);

          if (should_log) {
            spdlog::warn("Monitor {} status: {} (consecutive failures: {})",
                        state->get_destination().get_name(), to_string(new_status),
                        consecutive_failures);
          }
        }
      } catch (const std::exception& e) {
        spdlog::error("Critical error processing test result for {}: {}", state->get_destination().get_name(), e.what());
        // Create failure result for critical errors
        const test_result failure_result{false, 0, std::chrono::system_clock::now(), std::string("Critical error: ") + e.what()};
        try {
          state->add_result(failure_result);
        } catch (...) {
          spdlog::critical("Failed to record critical error result for {}", state->get_destination().get_name());
        }
      }
    });
  } catch (const std::exception& e) {
    spdlog::error("Error scheduling test for {}: {}", state->get_destination().get_name(), e.what());
  }
}

std::future<test_result> monitors::execute_test_async(const std::shared_ptr<monitor_state>& state) {
  // Create a packaged task for the test execution
  const auto task = std::make_shared<std::packaged_task<test_result()>>([state]() {
    try {
      const auto& dest = state->get_destination();
      spdlog::trace("Executing {} test for {}", to_string(dest.get_test().get_test_method()), dest.get_name());

      auto result = state->get_test_implementation()->execute(dest.get_test(), dest.get_timeout());

      spdlog::trace("Test {} for {} completed in {}ms: {}", to_string(dest.get_test().get_test_method()), dest.get_name(), result.get_duration_ms(),
                    result.is_success() ? "SUCCESS" : "FAILURE");

      return result;
    } catch (const std::exception& e) {
      spdlog::debug("Test failed for {}: {}", state->get_destination().get_name(), e.what());
      return test_result{false, 0, std::chrono::system_clock::now(), e.what()};
    }
  });

  auto future = task->get_future();

  // Execute the task (this returns immediately, task runs in background)
  (*task)();

  return future;
}

const std::map<std::string, std::shared_ptr<monitor_state>>& monitors::get_monitors_map() const { return monitors_map_; }

size_t monitors::active_tasks() const { return thread_pool_ ? thread_pool_->pending_tasks() : 0; }

size_t monitors::scheduled_tasks() const { return scheduler_ ? scheduler_->scheduled_count() : 0; }

void monitors::restart_failed_monitors() {
  if (!running_) {
    return;
  }

  spdlog::info("Performing health check and restarting failed monitors");

  size_t restart_count = 0;
  for (const auto& [key, state] : monitors_map_) {
    try {
      // Check if monitor has been failing for extended period
      const auto& dest = state->get_destination();
      if (monitor_status::failure == state->get_current_status() && state->get_consecutive_failures() > dest.get_failure() * 3) {
        spdlog::warn("Restarting severely failed monitor: {}", dest.get_name());

        // Reset the monitor state
        state->reset_consecutive_counts();

        // Reschedule the monitor test
        schedule_monitor_test(state);
        restart_count++;
      }
    } catch (const std::exception& e) {
      spdlog::error("Error during restart check for {}: {}", key, e.what());
    }
  }

  if (restart_count > 0) {
    spdlog::info("Restarted {} failed monitors", restart_count);
  }
}

bool monitors::is_healthy() const {
  if (!running_ || !thread_pool_ || !scheduler_) {
    return false;
  }

  // Check if thread pool is stopping
  if (thread_pool_->is_stopping()) {
    return false;
  }

  // Check if we have a reasonable number of active tasks
  const size_t pending = thread_pool_->pending_tasks();

  if (const size_t max_reasonable_pending = monitors_map_.size() * 2; pending > max_reasonable_pending) {
    spdlog::warn("High number of pending tasks: {} (monitors: {})", pending, monitors_map_.size());
    return false;
  }

  return true;
}
