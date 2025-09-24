#include "connection_tester.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <cstring>


test_result connection_tester_base::create_error_result(const std::string& error_msg, const long duration) {
    return test_result{false, duration, std::chrono::system_clock::now(), error_msg};
}

test_result connection_tester_base::create_success_result(const long duration) {
    return test_result{true, duration, std::chrono::system_clock::now(), std::nullopt};
}

bool connection_tester_base::resolve_address(const std::string& host, const int port, const int socket_type, addrinfo** result) {
    addrinfo hints{};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = socket_type;

    const int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, result);
    return status == 0;
}

// TCP Connection Tester Implementation
test_result tcp_connection_tester::test_connection(const std::string& host, const int port, const int timeout_ms) {
    const auto start_time = std::chrono::steady_clock::now();

    try {
        addrinfo* result;
        if (!resolve_address(host, port, SOCK_STREAM, &result)) {
            return create_error_result("DNS resolution failed");
        }

        bool connection_success = false;

        // Try connecting to each address returned by getaddrinfo
        for (const addrinfo* rp = result; rp != nullptr && !connection_success; rp = rp->ai_next) {
            connection_success = test_single_address(rp, timeout_ms);
        }

        freeaddrinfo(result);

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        if (connection_success) {
            return create_success_result(duration);
        } else {
            return create_error_result("Connection failed", duration);
        }

    } catch (const std::exception& e) {
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        return create_error_result(e.what(), duration);
    }
}

bool tcp_connection_tester::test_single_address(const addrinfo* addr_info, const int timeout_ms) {
    int sock = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
    if (sock < 0) {
        return false;
    }

    try {
        // Set socket to non-blocking mode
        const int flags = fcntl(sock, F_GETFL, 0);
        if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(sock);
            return false;
        }

        // Attempt connection
        int connect_result = connect(sock, addr_info->ai_addr, addr_info->ai_addrlen);

        if (connect_result == 0) {
            // Connection succeeded immediately
            close(sock);
            return true;
        } else if (errno == EINPROGRESS) {
            // Connection in progress, wait for completion
            fd_set write_fds, error_fds;
            FD_ZERO(&write_fds);
            FD_ZERO(&error_fds);
            FD_SET(sock, &write_fds);
            FD_SET(sock, &error_fds);

            timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            connect_result = select(sock + 1, nullptr, &write_fds, &error_fds, &tv);

            if (connect_result > 0) {
                if (FD_ISSET(sock, &error_fds)) {
                    // Connection failed
                    close(sock);
                    return false;
                } else if (FD_ISSET(sock, &write_fds)) {
                    // Check if connection was successful
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                        close(sock);
                        return true;
                    }
                }
            } else if (connect_result == 0) {
                // Timeout occurred
                spdlog::debug("TCP connection timeout");
            }
        } else {
            // Connection failed immediately
            spdlog::debug("TCP connection failed immediately: {}", strerror(errno));
        }
    } catch (...) {
        // Ensure socket is closed on exception
        close(sock);
        throw;
    }

    close(sock);
    return false;
}

// UDP Connection Tester Implementation
test_result udp_connection_tester::test_connection(const std::string& host, const int port, const int timeout_ms) {
    const auto start_time = std::chrono::steady_clock::now();

    try {
        addrinfo* result;
        if (!resolve_address(host, port, SOCK_DGRAM, &result)) {
            return create_error_result("DNS resolution failed");
        }

        bool send_success = false;

        // Try sending to each address returned by getaddrinfo
        for (const addrinfo* rp = result; rp != nullptr && !send_success; rp = rp->ai_next) {
            send_success = test_single_address(rp, timeout_ms);
        }

        freeaddrinfo(result);

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        if (send_success) {
            return create_success_result(duration);
        } else {
            return create_error_result("UDP send failed", duration);
        }

    } catch (const std::exception& e) {
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        return create_error_result(e.what(), duration);
    }
}

bool udp_connection_tester::test_single_address(const addrinfo* addr_info, const int timeout_ms) {
    int sock = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
    if (sock < 0) {
        return false;
    }

    try {
        // Set socket timeout for send operation
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            close(sock);
            return false;
        }

        // Send empty UDP packet
        constexpr char buffer[1] = {0};
        const ssize_t sent = sendto(sock, buffer, 0, 0, addr_info->ai_addr, addr_info->ai_addrlen);

        // For UDP, we consider it successful if no error occurred during send
        const bool success = sent >= 0;

        close(sock);
        return success;

    } catch (...) {
        // Ensure socket is closed on exception
        close(sock);
        throw;
    }
}

// Factory Implementation
std::unique_ptr<connection_tester_base> connection_tester_factory::create(const protocol proto) {
    switch (proto) {
        case protocol::tcp:
            return std::make_unique<tcp_connection_tester>();
        case protocol::udp:
            return std::make_unique<udp_connection_tester>();
        default:
            throw std::invalid_argument("Unsupported protocol for connection test");
    }
}