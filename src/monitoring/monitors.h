#pragma once

#include <atomic>
#include <future>
#include <map>
#include <memory>
#include <vector>

#include "../core/types.h"
#include "../utils/async_scheduler.h"
#include "../utils/thread_pool.h"
#include "monitor_config_types.h"
#include "monitor_state.h"

class monitors {
 public:
  explicit monitors(const monitor_config& config);
  ~monitors();

  void start_monitoring();
  void stop_monitoring();

  [[nodiscard]] const std::map<std::string, std::shared_ptr<monitor_state>>& get_monitors_map() const;

  // Performance metrics
  [[nodiscard]] size_t active_tasks() const;
  [[nodiscard]] size_t scheduled_tasks() const;

  // Error recovery
  void restart_failed_monitors();
  [[nodiscard]] bool is_healthy() const;

  // Get the shared thread pool for other components
  [[nodiscard]] std::shared_ptr<thread_pool> get_thread_pool() const { return thread_pool_; }

 private:
  std::map<std::string, std::shared_ptr<monitor_state>> monitors_map_;
  std::shared_ptr<thread_pool> thread_pool_;
  std::unique_ptr<async_scheduler> scheduler_;
  std::vector<size_t> scheduled_task_ids_;
  std::atomic<bool> running_;

  // Async test execution
  void perform_test_async(const std::shared_ptr<monitor_state>& state) const;
  static std::future<test_result> execute_test_async(const std::shared_ptr<monitor_state>& state);
  void schedule_monitor_test(const std::shared_ptr<monitor_state>& state);
};