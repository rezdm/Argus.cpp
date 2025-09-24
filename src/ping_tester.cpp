#include "ping_tester.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netdb.h>
#include <unistd.h>
#include <chrono>
#include <regex>
#include <cstring>
#include <sstream>

test_result ping_tester_base::create_error_result(const std::string& error_msg, const long duration) {
    return test_result{false, duration, std::chrono::system_clock::now(), error_msg};
}

test_result ping_tester_base::create_success_result(const long duration) {
    return test_result{true, duration, std::chrono::system_clock::now(), std::nullopt};
}

// System Ping Tester Implementation
test_result system_ping_tester::ping_host(const std::string& host, const int timeout_ms) {
    const auto start_time = std::chrono::steady_clock::now();

    try {
        const std::string command = build_ping_command(host, timeout_ms);

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return create_error_result("Failed to execute ping command");
        }

        // Read ping output
        std::string output;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }

        const int result_code = pclose(pipe);

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        if (result_code == 0 && parse_ping_output(output)) {
            return create_success_result(duration);
        } else {
            return create_error_result("Ping failed or host unreachable", duration);
        }

    } catch (const std::exception& e) {
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        return create_error_result(e.what(), duration);
    }
}

std::string system_ping_tester::build_ping_command(const std::string& host, const int timeout_ms) {
    std::ostringstream cmd;

    // Use ping for IPv4, ping6 for IPv6 (if needed)
    cmd << "ping -c 1 -W " << (timeout_ms / 1000 + 1) << " ";

    // Add host (with basic validation to prevent injection)
    if (host.find_first_of(";&|`$(){}[]<>") != std::string::npos) {
        throw std::invalid_argument("Invalid characters in hostname");
    }

    cmd << "'" << host << "' 2>/dev/null";

    return cmd.str();
}

bool system_ping_tester::parse_ping_output(const std::string& output) {
    // Look for patterns that indicate successful ping
    const std::regex success_patterns[] = {
        std::regex(R"(\d+ bytes from)"),         // Standard ping success
        std::regex(R"(\d+ packets transmitted, \d+ received)"), // Summary line
        std::regex(R"(time=\d+\.?\d*ms)")        // Time measurement
    };

    for (const auto& pattern : success_patterns) {
        if (std::regex_search(output, pattern)) {
            return true;
        }
    }

    return false;
}

// ICMP Ping Tester Implementation
icmp_ping_tester::icmp_ping_tester() : icmp_socket_(-1), socket_initialized_(false) {
    socket_initialized_ = initialize_socket();
    if (!socket_initialized_) {
        spdlog::warn("Failed to initialize ICMP socket. ICMP ping tests will fail.");
    }
}

icmp_ping_tester::~icmp_ping_tester() {
    cleanup_socket();
}

test_result icmp_ping_tester::ping_host(const std::string& host, const int timeout_ms) {
    const auto start_time = std::chrono::steady_clock::now();

    if (!socket_initialized_) {
        return create_error_result("ICMP socket not initialized");
    }

    try {
        // Resolve hostname
        addrinfo hints{}, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // IPv4 for now
        hints.ai_socktype = SOCK_RAW;

        const int status = getaddrinfo(host.c_str(), nullptr, &hints, &result);
        if (status != 0) {
            return create_error_result("DNS resolution failed");
        }

        bool ping_success = false;

        // Try pinging each resolved address
        for (const addrinfo* rp = result; rp != nullptr && !ping_success; rp = rp->ai_next) {
            if (send_icmp_packet(icmp_socket_, rp->ai_addr, rp->ai_addrlen)) {
                ping_success = wait_for_reply(icmp_socket_, timeout_ms);
            }
        }

        freeaddrinfo(result);

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        if (ping_success) {
            return create_success_result(duration);
        } else {
            return create_error_result("ICMP ping failed", duration);
        }

    } catch (const std::exception& e) {
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        return create_error_result(e.what(), duration);
    }
}

bool icmp_ping_tester::initialize_socket() {
    // Try to create an unprivileged ICMP socket
    icmp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (icmp_socket_ < 0) {
        spdlog::debug("Failed to create unprivileged ICMP socket: {}", strerror(errno));
        return false;
    }

    return true;
}

bool icmp_ping_tester::send_icmp_packet(const int socket, const sockaddr* dest_addr, const socklen_t addr_len) {
    // For unprivileged ICMP, we send a minimal packet
    constexpr char packet[8] = {0}; // Minimal ICMP packet

    const ssize_t sent = sendto(socket, packet, sizeof(packet), 0, dest_addr, addr_len);
    return sent > 0;
}

bool icmp_ping_tester::wait_for_reply(const int socket, const int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket, &read_fds);

    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    const int result = select(socket + 1, &read_fds, nullptr, nullptr, &tv);

    if (result > 0 && FD_ISSET(socket, &read_fds)) {
        // Try to read the reply (we don't actually parse it for simplicity)
        char buffer[1024];
        const ssize_t received = recv(socket, buffer, sizeof(buffer), 0);
        return received > 0;
    }

    return false; // Timeout or error
}

void icmp_ping_tester::cleanup_socket() {
    if (icmp_socket_ >= 0) {
        close(icmp_socket_);
        icmp_socket_ = -1;
    }
    socket_initialized_ = false;
}

// Factory Implementation
std::unique_ptr<ping_tester_base> ping_tester_factory::create(const ping_implementation impl) {
    switch (impl) {
        case ping_implementation::system_ping:
            return std::make_unique<system_ping_tester>();
        case ping_implementation::unprivileged_icmp:
            return std::make_unique<icmp_ping_tester>();
        default:
            throw std::invalid_argument("Unsupported ping implementation");
    }
}