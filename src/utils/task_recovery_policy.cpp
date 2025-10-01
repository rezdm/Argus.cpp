#include "task_recovery_policy.h"

#include <spdlog/spdlog.h>

// Fixed Delay Recovery Implementation
fixed_delay_recovery::fixed_delay_recovery(const std::chrono::seconds retry_delay, const int max_retries) : retry_delay_(retry_delay), max_retries_(max_retries) {}

std::optional<std::chrono::seconds> fixed_delay_recovery::should_retry(const size_t task_id, const std::string& error_message, const int failure_count) const {
  if (failure_count >= max_retries_) {
    spdlog::error("Task {} exceeded max retries ({}): {}", task_id, max_retries_, error_message);
    return std::nullopt;
  }

  spdlog::info("Task {} failed (attempt {}/{}), will retry in {}s: {}", task_id, failure_count + 1, max_retries_, retry_delay_.count(), error_message);
  return retry_delay_;
}

void fixed_delay_recovery::on_recovery_success(const size_t task_id) const { spdlog::debug("Task {} recovered successfully", task_id); }

void fixed_delay_recovery::on_recovery_abandoned(const size_t task_id, const std::string& reason) const { spdlog::warn("Task {} recovery abandoned: {}", task_id, reason); }

// No Recovery Implementation
std::optional<std::chrono::seconds> no_recovery::should_retry(const size_t task_id, const std::string& error_message, [[maybe_unused]] const int failure_count) const {
  spdlog::error("Task {} failed: {}. No recovery attempted.", task_id, error_message);
  return std::nullopt;
}

void no_recovery::on_recovery_success([[maybe_unused]] const size_t task_id) const {
  // No-op
}

void no_recovery::on_recovery_abandoned([[maybe_unused]] const size_t task_id, [[maybe_unused]] const std::string& reason) const {
  // No-op
}
