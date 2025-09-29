#include "network_test_ping.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <optional>

#include "../core/constants.h"
#include "../core/logging.h"
#include "../monitoring/test_config.h"
#include "../utils/test_result.h"
#include "ping_tester.h"

test_result network_test_ping::execute(const test_config& config, const int timeout_ms) const {
  const auto start_time = std::chrono::steady_clock::now();
  bool success = false;
  std::string error;

  try {
    validate_config(config);

    // Validate timeout
    if (timeout_ms <= 0 || timeout_ms > argus::constants::MAX_PING_TIMEOUT_MS) {
      throw std::invalid_argument("Invalid timeout: must be between 1ms and 300000ms");
    }

    // Use auto-fallback ping tester
    const auto tester = ping_tester_factory::create_auto_fallback();
    const auto result = tester->ping_host(config.get_host().value(), timeout_ms);
    success = result.is_success();

    if (!success && result.has_error()) {
      error = result.get_error().value();
    } else if (!success) {
      error = "Host unreachable";
    }

  } catch (const std::exception& e) {
    error = e.what();
    std::string host_str = config.get_host().value_or("unknown");
    LOG_TEST_FAILURE("Ping", host_str, error);
  }

  const auto end_time = std::chrono::steady_clock::now();
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  return {success, duration, std::chrono::system_clock::now(), error.empty() ? std::nullopt : std::optional(error)};
}

std::string network_test_ping::get_description(const test_config& config) const { return "PING " + config.get_host().value_or("unknown"); }

void network_test_ping::validate_config(const test_config& config) const {
  if (!config.get_host() || config.get_host()->empty()) {
    throw std::invalid_argument("Host is required for ping test");
  }
}
