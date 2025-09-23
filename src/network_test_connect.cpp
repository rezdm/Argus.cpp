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
    struct addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP socket

    // Resolve hostname for both IPv4 and IPv6
    const int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (status != 0) {
        return false;
    }

    bool connection_success = false;

    // Try connecting to each address returned by getaddrinfo
    for (struct addrinfo* rp = result; rp != nullptr && !connection_success; rp = rp->ai_next) {
        const int sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            continue;
        }

        // Set socket to non-blocking mode
        const int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        // Attempt connection
        int connect_result = connect(sock, rp->ai_addr, rp->ai_addrlen);

        if (connect_result == 0) {
            // Connection succeeded immediately
            connection_success = true;
        } else if (errno == EINPROGRESS) {
            // Connection in progress, wait for completion
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);

            timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            connect_result = select(sock + 1, nullptr, &write_fds, nullptr, &tv);

            if (connect_result > 0) {
                // Check if connection was successful
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                    connection_success = true;
                }
            }
        }

        close(sock);
    }

    freeaddrinfo(result);
    return connection_success;
}

bool network_test_connect::test_udp_connection(const std::string& host, const int port, const int timeout_ms) {
    // Note: timeout_ms is not easily applicable to UDP since it's connectionless
    (void)timeout_ms; // Suppress warning

    struct addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    // Resolve hostname for both IPv4 and IPv6
    const int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (status != 0) {
        return false;
    }

    bool send_success = false;

    // Try sending to each address returned by getaddrinfo
    for (struct addrinfo* rp = result; rp != nullptr && !send_success; rp = rp->ai_next) {
        const int sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            continue;
        }

        // Send empty UDP packet
        const char buffer[1] = {0};
        const ssize_t sent = sendto(sock, buffer, 0, 0, rp->ai_addr, rp->ai_addrlen);

        close(sock);

        // For UDP, we consider it successful if no error occurred during send
        if (sent >= 0) {
            send_success = true;
        }
    }

    freeaddrinfo(result);
    return send_success;
}

