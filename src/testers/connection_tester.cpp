#include "connection_tester.h"
#include "../network/address_family_handler.h"
#include "../utils/test_result.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <optional>


test_result connection_tester_base::create_error_result(const std::string& error_msg, const long duration) {
    return test_result{false, duration, std::chrono::system_clock::now(), error_msg};
}

test_result connection_tester_base::create_success_result(const long duration) {
    return test_result{true, duration, std::chrono::system_clock::now(), std::nullopt};
}


// TCP Connection Tester Implementation
test_result tcp_connection_tester::test_connection(const std::string& host, const int port, const int timeout_ms) {
    const auto start_time = std::chrono::steady_clock::now();

    try {
        // Use address family resolver with IPv6 preferred strategy
        const auto resolver = address_family_factory::create_resolver(address_family_preference::ipv6_preferred);
        const auto addresses = resolver->resolve_with_preference(host, port, SOCK_STREAM);

        if (addresses.empty()) {
            return create_error_result("DNS resolution failed for all address families");
        }

        spdlog::debug("Resolved {} addresses for {}:{}", addresses.size(), host, port);

        // Try each address with its appropriate handler
        for (const auto& addr : addresses) {
            if (test_single_address(addr, timeout_ms)) {
                const auto end_time = std::chrono::steady_clock::now();
                const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

                spdlog::debug("TCP connection succeeded to {} ({})", addr.display_name, addr.family == AF_INET ? "IPv4" : "IPv6");
                return create_success_result(duration);
            }
        }

        // All connections failed
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        return create_error_result("Connection failed to all resolved addresses", duration);

    } catch (const std::exception& e) {
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        return create_error_result(e.what(), duration);
    }
}


bool tcp_connection_tester::test_single_address(const resolved_address& addr, int timeout_ms) {
    try {
        // Get the appropriate address family handler
        std::unique_ptr<address_family_handler_base> handler;
        if (addr.family == AF_INET) {
            handler = address_family_factory::create_ipv4_handler();
        } else if (addr.family == AF_INET6) {
            handler = address_family_factory::create_ipv6_handler();
        } else {
            spdlog::debug("Unsupported address family: {}", addr.family);
            return false;
        }

        // Create socket using the handler
        const int sock = handler->create_socket(addr);
        if (sock < 0) {
            return false;
        }

        try {
            // Configure socket using handler
            if (!handler->configure_socket(sock, timeout_ms)) {
                close(sock);
                return false;
            }

            // Set socket to non-blocking mode for connect timeout
            if (const int flags = fcntl(sock, F_GETFL, 0); flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
                close(sock);
                return false;
            }

            // Attempt connection
            const auto* sock_addr = reinterpret_cast<const sockaddr*>(&addr.addr);

            if (int connect_result = connect(sock, sock_addr, addr.addr_len); connect_result == 0) {
                // Connection succeeded immediately
                spdlog::trace("Immediate {} TCP connection to {}", handler->get_family_name(), addr.display_name);
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
                        spdlog::trace("{} TCP connection failed to {}: Socket error", handler->get_family_name(), addr.display_name);
                        close(sock);
                        return false;
                    } else if (FD_ISSET(sock, &write_fds)) {
                        // Check if connection was successful
                        int error = 0;
                        socklen_t len = sizeof(error);
                        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                            spdlog::trace("{} TCP connection succeeded to {}", handler->get_family_name(), addr.display_name);
                            close(sock);
                            return true;
                        }
                    }
                } else if (connect_result == 0) {
                    spdlog::trace("{} TCP connection timeout to {}", handler->get_family_name(), addr.display_name);
                } else {
                    spdlog::trace("{} TCP connection select error to {}: {}", handler->get_family_name(), addr.display_name, strerror(errno));
                }
            } else {
                spdlog::trace("{} TCP connection failed immediately to {}: {}", handler->get_family_name(), addr.display_name, strerror(errno));
            }

            close(sock);
            return false;

        } catch (...) {
            close(sock);
            throw;
        }

    } catch (const std::exception& e) {
        spdlog::debug("Exception in TCP connection test to {}: {}", addr.display_name, e.what());
        return false;
    }
}

// UDP Connection Tester Implementation
test_result udp_connection_tester::test_connection(const std::string& host, const int port, const int timeout_ms) {
    const auto start_time = std::chrono::steady_clock::now();

    try {
        // Use address family resolver with IPv6 preferred strategy
        const auto resolver = address_family_factory::create_resolver(address_family_preference::ipv6_preferred);
        const auto addresses = resolver->resolve_with_preference(host, port, SOCK_DGRAM);

        if (addresses.empty()) {
            return create_error_result("DNS resolution failed for all address families");
        }

        spdlog::debug("Resolved {} addresses for {}:{}", addresses.size(), host, port);

        // Try each address with its appropriate handler
        for (const auto& addr : addresses) {
            if (test_single_address(addr, timeout_ms)) {
                const auto end_time = std::chrono::steady_clock::now();
                const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

                spdlog::debug("UDP send succeeded to {} ({})", addr.display_name, addr.family == AF_INET ? "IPv4" : "IPv6");
                return create_success_result(duration);
            }
        }

        // All UDP sends failed
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        return create_error_result("UDP send failed to all resolved addresses", duration);

    } catch (const std::exception& e) {
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        return create_error_result(e.what(), duration);
    }
}


bool udp_connection_tester::test_single_address(const resolved_address& addr, int timeout_ms) {
    try {
        // Get the appropriate address family handler
        std::unique_ptr<address_family_handler_base> handler;
        if (addr.family == AF_INET) {
            handler = address_family_factory::create_ipv4_handler();
        } else if (addr.family == AF_INET6) {
            handler = address_family_factory::create_ipv6_handler();
        } else {
            spdlog::debug("Unsupported address family for UDP: {}", addr.family);
            return false;
        }

        // Create socket using the handler
        const int sock = handler->create_socket(addr);
        if (sock < 0) {
            return false;
        }

        try {
            // Configure socket using handler
            if (!handler->configure_socket(sock, timeout_ms)) {
                close(sock);
                return false;
            }

            // Send empty UDP packet
            constexpr char buffer[1] = {0};
            const auto* sock_addr = reinterpret_cast<const sockaddr*>(&addr.addr);
            const ssize_t sent = sendto(sock, buffer, 0, 0, sock_addr, addr.addr_len);

            // For UDP, we consider it successful if no error occurred during send
            const bool success = sent >= 0;

            if (success) {
                spdlog::trace("{} UDP send succeeded to {}", handler->get_family_name(), addr.display_name);
            } else {
                spdlog::trace("{} UDP send failed to {}: {}", handler->get_family_name(), addr.display_name, strerror(errno));
            }

            close(sock);
            return success;

        } catch (...) {
            close(sock);
            throw;
        }

    } catch (const std::exception& e) {
        spdlog::debug("Exception in UDP send test to {}: {}", addr.display_name, e.what());
        return false;
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