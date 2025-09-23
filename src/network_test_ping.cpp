#include "network_test_ping.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <regex>
#include <cctype>

test_result network_test_ping::execute(const test_config& config, const int timeout_ms) const {
    const auto start_time = std::chrono::steady_clock::now();
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

void network_test_ping::set_ping_implementation(ping_implementation impl) {
    ping_impl_ = impl;
}

ping_implementation network_test_ping::get_ping_implementation() {
    return ping_impl_;
}

bool network_test_ping::ping_host(const std::string& host, const int timeout_ms) {
    switch (ping_impl_) {
        case ping_implementation::system_ping:
            return ping_system_command(host, timeout_ms);
        case ping_implementation::unprivileged_icmp:
            return ping_unprivileged_icmp(host, timeout_ms);
        default:
            spdlog::error("Unknown ping implementation: {}", static_cast<int>(ping_impl_));
            return false;
    }
}

bool network_test_ping::is_valid_hostname(const std::string& host) {
    if (host.empty() || host.length() > 255) {
        return false;
    }

    // Check for valid hostname/IP format
    // Allow alphanumeric, dots, hyphens, and underscores
    const std::regex hostname_regex("^[a-zA-Z0-9._-]+$");
    if (!std::regex_match(host, hostname_regex)) {
        return false;
    }

    // Additional checks can be added here (e.g., IP validation)
    return true;
}

std::string network_test_ping::escape_shell_arg(const std::string& arg) {
    std::string escaped;
    for (const char c : arg) {
        // Only allow safe characters, escape others
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_') {
            escaped += c;
        } else {
            // For any unsafe character, we'll reject the input
            throw std::invalid_argument("Invalid character in hostname: " + std::string(1, c));
        }
    }
    return escaped;
}

bool network_test_ping::ping_system_command(const std::string& host, const int timeout_ms) {
    if (!is_valid_hostname(host)) {
        spdlog::warn("Invalid hostname for ping: {}", host);
        return false;
    }

    try {
        const std::string safe_host = escape_shell_arg(host);
        const std::string command = "ping -c 1 -W " + std::to_string(timeout_ms / 1000 + 1) + " '" + safe_host + "' > /dev/null 2>&1";
        const int result = system(command.c_str());
        return result == 0;
    } catch (const std::exception& e) {
        spdlog::error("Failed to ping {}: {}", host, e.what());
        return false;
    }
}

bool network_test_ping::ping_unprivileged_icmp(const std::string& host, const int timeout_ms) {
    if (!is_valid_hostname(host)) {
        spdlog::warn("Invalid hostname for ICMP ping: {}", host);
        return false;
    }

    // Create unprivileged ICMP socket
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0) {
        spdlog::warn("Failed to create unprivileged ICMP socket: {}. Consider enabling with: sysctl -w net.ipv4.ping_group_range='0 65535'", strerror(errno));
        return false;
    }

    // Set socket timeout
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        close(sock);
        return false;
    }

    // Resolve hostname
    const hostent* he = gethostbyname(host.c_str());
    if (!he) {
        close(sock);
        return false;
    }

    // Set up destination address
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    // Send ICMP echo request
    const char ping_data[] = "ping";
    if (sendto(sock, ping_data, sizeof(ping_data), 0, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    // Try to receive response
    char buffer[1024];
    socklen_t addr_len = sizeof(addr);
    const ssize_t received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addr_len);

    close(sock);
    return received > 0;
}

