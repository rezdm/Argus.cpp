#include "network_test_ping.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <chrono>

test_result network_test_ping::execute(const test_config& config, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    std::string error;

    try {
        validate_config(config);
        success = ping_host(config.host.value(), timeout_ms);
        
        if (!success) {
            error = "Host unreachable";
        }
    } catch (const std::exception& e) {
        error = e.what();
        spdlog::debug("Ping test failed for {}: {}", config.host.value_or("unknown"), error);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return test_result(success, duration, std::chrono::system_clock::now(), 
                      error.empty() ? std::nullopt : std::optional<std::string>(error));
}

std::string network_test_ping::get_description(const test_config& config) {
    return "PING " + config.host.value_or("unknown");
}

void network_test_ping::validate_config(const test_config& config) {
    if (!config.host || config.host->empty()) {
        throw std::invalid_argument("Host is required for ping test");
    }
}

bool network_test_ping::ping_host(const std::string& host, int timeout_ms) {
    // Simple implementation using system ping command for portability
    // In production, you might want to implement raw ICMP sockets
    
    std::string command = "ping -c 1 -W " + std::to_string(timeout_ms / 1000 + 1) + " " + host + " > /dev/null 2>&1";
    int result = system(command.c_str());
    return result == 0;
}