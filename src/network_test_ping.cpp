#include "network_test_ping.h"
#include "ping_tester.h"
#include <spdlog/spdlog.h>
#include <chrono>

test_result network_test_ping::execute(const test_config& config, const int timeout_ms) const {
    const auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    std::string error;

    try {
        validate_config(config);

        // Validate timeout
        if (timeout_ms <= 0 || timeout_ms > 300000) { // Max 5 minutes
            throw std::invalid_argument("Invalid timeout: must be between 1ms and 300000ms");
        }

        // Use factory pattern to create appropriate ping tester
        auto tester = ping_tester_factory::create(ping_impl_);
        auto result = tester->ping_host(config.host.value(), timeout_ms);
        success = result.success;

        if (!success && result.error.has_value()) {
            error = result.error.value();
        } else if (!success) {
            error = "Host unreachable";
        }

    } catch (const std::exception& e) {
        error = e.what();
        std::string host_str = config.host.value_or("unknown");
        spdlog::debug("Ping test failed for {}: {}", host_str, error);
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return {success, duration, std::chrono::system_clock::now(), error.empty() ? std::nullopt : std::optional(error)};
}

std::string network_test_ping::get_description(const test_config& config) const {
    return "PING " + config.host.value_or("unknown");
}

void network_test_ping::validate_config(const test_config& config) const {
    if (!config.host || config.host->empty()) {
        throw std::invalid_argument("Host is required for ping test");
    }
}

// Static member definition
ping_implementation network_test_ping::ping_impl_ = ping_implementation::system_ping;

void network_test_ping::set_ping_implementation(const ping_implementation impl) {
    ping_impl_ = impl;
}

ping_implementation network_test_ping::get_ping_implementation() {
    return ping_impl_;
}



