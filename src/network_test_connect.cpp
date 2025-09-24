#include "network_test_connect.h"
#include "connection_tester.h"
#include <spdlog/spdlog.h>
#include <chrono>

test_result network_test_connect::execute(const test_config& config, const int timeout_ms) const {
    const auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    std::string error;

    try {
        validate_config(config);

        // Validate timeout
        if (timeout_ms <= 0 || timeout_ms > 300000) { // Max 5 minutes
            throw std::invalid_argument("Invalid timeout: must be between 1ms and 300000ms");
        }

        // Use factory pattern to create appropriate connection tester
        auto tester = connection_tester_factory::create(config.protocol_type.value());
        auto result = tester->test_connection(config.host.value(), config.port, timeout_ms);
        success = result.success;

    } catch (const std::exception& e) {
        error = e.what();
        std::string host_str = config.host.value_or("unknown");
        std::string protocol_str = to_string(config.protocol_type.value());
        spdlog::debug("Connection test failed for {}:{} ({}): {}", host_str, config.port, protocol_str, error);
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return {success, duration, std::chrono::system_clock::now(), error.empty() ? std::nullopt : std::optional(error)};
}

std::string network_test_connect::get_description(const test_config& config) const {
    return config.host.value_or("unknown") + ":" + std::to_string(config.port) + " (" + to_string(config.protocol_type.value()) + ")";
}

void network_test_connect::validate_config(const test_config& config) const {
    if (!config.host || config.host->empty()) {
        throw std::invalid_argument("Host is required for connection test");
    }
    if (config.port <= 0 || config.port > 65535) {
        throw std::invalid_argument("Valid port (1-65535) is required for connection test");
    }
    if (!config.protocol_type) {
        throw std::invalid_argument("Protocol must be 'tcp' or 'udp' for connection test");
    }
}


