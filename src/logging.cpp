#include "logging.h"
#include "constants.h"

namespace argus {
namespace logging {

void Logger::log_startup(const std::string& config_path) {
    spdlog::info("Starting {} Monitor with config: {}", constants::APPLICATION_NAME, config_path);
}

void Logger::log_shutdown() {
    spdlog::info("Shutting down {} Monitor", constants::APPLICATION_NAME);
}

void Logger::log_memory_usage(const std::string& phase) {
    spdlog::info("Memory [{}]: Phase completed", phase);
}

void Logger::log_config_loaded(const std::string& instance_name) {
    spdlog::info("Loaded configuration for instance: {}", instance_name);
}

void Logger::log_config_reload_start(const std::string& config_path) {
    spdlog::info("Starting configuration reload from: {}", config_path);
}

void Logger::log_config_reload_success() {
    spdlog::info("Configuration reload completed successfully");
}

void Logger::log_config_reload_failure(const std::string& error) {
    spdlog::error("Configuration reload failed: {}. Continuing with current configuration.", error);
}

void Logger::log_component_start(const std::string& component_name) {
    spdlog::info("{} started successfully", component_name);
}

void Logger::log_component_stop(const std::string& component_name) {
    spdlog::info("Stopping {} for reload", component_name);
}

void Logger::log_component_failure(const std::string& component_name, const std::string& error) {
    spdlog::error("Failed to initialize {}: {}. Continuing with reduced functionality.", component_name, error);
}

void Logger::log_test_failure(const std::string& test_type, const std::string& target, const std::string& error) {
    spdlog::debug("{} test failed for {}: {}", test_type, target, error);
}

void Logger::log_network_debug(const std::string& operation, const std::string& target, const std::string& message) {
    spdlog::debug("{} failed for {}: {}", operation, target, message);
}

void Logger::log_performance(const std::string& operation, long duration_ms) {
    spdlog::debug("{} completed in {}ms", operation, duration_ms);
}

void Logger::log_error_with_context(const std::string& operation, const std::string& context, const std::string& error) {
    spdlog::error("Failed to {} for {}: {}", operation, context, error);
}

void Logger::log_warning_with_context(const std::string& operation, const std::string& context, const std::string& message) {
    spdlog::warn("{} warning for {}: {}", operation, context, message);
}

void Logger::log_systemd_operation(const std::string& operation, bool success) {
    if (success) {
        spdlog::info("Notified systemd of {}", operation);
    } else {
        spdlog::warn("Failed to notify systemd of {}", operation);
    }
}

void Logger::log_daemon_operation(const std::string& operation, bool success) {
    if (success) {
        spdlog::info("Daemon {}: successful", operation);
    } else {
        spdlog::error("Daemon {}: failed", operation);
    }
}

} // namespace logging
} // namespace argus