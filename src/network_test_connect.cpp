#include "network_test_connect.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <cstring>

test_result network_test_connect::execute(const test_config& config, const int timeout_ms) const {
    const auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    std::string error;

    try {
        validate_config(config);
        
        switch (config.protocol_type.value()) {
            case protocol::tcp:
                success = test_tcp_connection(config.host.value(), config.port, timeout_ms);
                break;
            case protocol::udp:
                success = test_udp_connection(config.host.value(), config.port, timeout_ms);
                break;
            default:
                throw std::invalid_argument("Unknown protocol");
        }
        
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

bool network_test_connect::test_tcp_connection(const std::string& host, const int port, const int timeout_ms) {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }

    // Set socket to non-blocking mode
    const int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // Resolve hostname
    const hostent* he = gethostbyname(host.c_str());
    if (!he) {
        close(sock);
        return false;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    // Attempt connection
    int result = connect(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    
    if (result == 0) {
        // Connection succeeded immediately
        close(sock);
        return true;
    }

    if (errno == EINPROGRESS) {
        // Connection in progress, wait for completion
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);

        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        result = select(sock + 1, nullptr, &write_fds, nullptr, &tv);
        
        if (result > 0) {
            // Check if connection was successful
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                close(sock);
                return true;
            }
        }
    }

    close(sock);
    return false;
}

bool network_test_connect::test_udp_connection(const std::string& host, const int port, const int timeout_ms) {
    // Note: timeout_ms is not easily applicable to UDP since it's connectionless
    (void)timeout_ms; // Suppress warning

    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }

    sockaddr_in addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // Resolve hostname
    const hostent* he = gethostbyname(host.c_str());
    if (!he) {
        close(sock);
        return false;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    // Send empty UDP packet
    const char buffer[1] = {0};
    const ssize_t sent = sendto(sock, buffer, 0, 0, (struct sockaddr*)&addr, sizeof(addr));
    
    close(sock);
    
    // For UDP, we consider it successful if no error occurred during send
    return sent >= 0;
}

