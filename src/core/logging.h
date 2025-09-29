#pragma once

#include <spdlog/spdlog.h>

#include <string>

namespace argus::logging {

// Centralized logging functions with consistent patterns
class logger {
 public:
  // Application lifecycle logging
  static void log_startup(const std::string& config_path);
  static void log_shutdown();
  static void log_memory_usage(const std::string& phase);

  // Configuration logging
  static void log_config_loaded(const std::string& instance_name);
  static void log_config_reload_start(const std::string& config_path);
  static void log_config_reload_success();
  static void log_config_reload_failure(const std::string& error);

  // Component lifecycle logging
  static void log_component_start(const std::string& component_name);
  static void log_component_stop(const std::string& component_name);
  static void log_component_failure(const std::string& component_name, const std::string& error);

  // Network test logging
  static void log_test_failure(const std::string& test_type, const std::string& target, const std::string& error);
  static void log_network_debug(const std::string& operation, const std::string& target, const std::string& message);

  // Performance logging
  static void log_performance(const std::string& operation, long duration_ms);

  // Error logging with context
  static void log_error_with_context(const std::string& operation, const std::string& context, const std::string& error);
  static void log_warning_with_context(const std::string& operation, const std::string& context, const std::string& message);

  // System logging
  static void log_systemd_operation(const std::string& operation, bool success);
  static void log_daemon_operation(const std::string& operation, bool success);
};

// Convenience macros for consistent logging patterns
#define LOG_STARTUP(config) argus::logging::logger::log_startup(config)
#define LOG_SHUTDOWN() argus::logging::logger::log_shutdown()
#define LOG_COMPONENT_START(name) argus::logging::logger::log_component_start(name)
#define LOG_COMPONENT_STOP(name) argus::logging::logger::log_component_stop(name)
#define LOG_COMPONENT_FAILURE(name, error) argus::logging::logger::log_component_failure(name, error)
#define LOG_TEST_FAILURE(type, target, error) argus::logging::logger::log_test_failure(type, target, error)
#define LOG_NETWORK_DEBUG(op, target, msg) argus::logging::logger::log_network_debug(op, target, msg)
#define LOG_ERROR_CTX(op, ctx, error) argus::logging::logger::log_error_with_context(op, ctx, error)
#define LOG_WARNING_CTX(op, ctx, msg) argus::logging::logger::log_warning_with_context(op, ctx, msg)

}  // namespace argus::logging
