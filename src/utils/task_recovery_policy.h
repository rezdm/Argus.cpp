#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>

// Policy for handling task execution failures
class task_recovery_policy {
 public:
  virtual ~task_recovery_policy() = default;

  // Decide whether to retry a failed task
  // Returns the delay before retry, or std::nullopt if no retry
  [[nodiscard]] virtual std::optional<std::chrono::seconds> should_retry(size_t task_id, const std::string& error_message, int failure_count) const = 0;

  // Called when a task is successfully recovered
  virtual void on_recovery_success(size_t task_id) const = 0;

  // Called when recovery is abandoned
  virtual void on_recovery_abandoned(size_t task_id, const std::string& reason) const = 0;
};

// Simple recovery: retry repeating tasks with fixed delay
class fixed_delay_recovery final : public task_recovery_policy {
 public:
  explicit fixed_delay_recovery(std::chrono::seconds retry_delay = std::chrono::seconds(10), int max_retries = 3);

  [[nodiscard]] std::optional<std::chrono::seconds> should_retry(size_t task_id, const std::string& error_message, int failure_count) const override;
  void on_recovery_success(size_t task_id) const override;
  void on_recovery_abandoned(size_t task_id, const std::string& reason) const override;

 private:
  std::chrono::seconds retry_delay_;
  int max_retries_;
};

// No recovery - just log and continue
class no_recovery final : public task_recovery_policy {
 public:
  [[nodiscard]] std::optional<std::chrono::seconds> should_retry(size_t task_id, const std::string& error_message, int failure_count) const override;
  void on_recovery_success(size_t task_id) const override;
  void on_recovery_abandoned(size_t task_id, const std::string& reason) const override;
};
