#pragma once

#include <future>
#include <string>

#include "../core/types.h"

// Async version of network test interface
class network_test_async {
 public:
  virtual ~network_test_async() = default;

  // Execute test asynchronously and return a future
  [[nodiscard]] virtual std::future<test_result> execute_async(const test_config& config, int timeout_ms) const = 0;

  // Synchronous wrapper for backward compatibility
  [[nodiscard]] virtual test_result execute(const test_config& config, int timeout_ms) const;

  [[nodiscard]] virtual std::string get_description(const test_config& config) const = 0;
  virtual void validate_config(const test_config& config) const = 0;
};