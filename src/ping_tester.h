#pragma once

#include "types.h"
#include <string>
#include <memory>
#include <vector>

enum class ping_implementation {
    system_ping,
    unprivileged_icmp,
    raw_socket
};

class ping_tester_base {
public:
    virtual ~ping_tester_base() = default;
    virtual test_result ping_host(const std::string& host, int timeout_ms) = 0;
    [[nodiscard]] virtual ping_implementation get_implementation_type() const = 0;

protected:
    static test_result create_error_result(const std::string& error_msg, long duration = 0);
    static test_result create_success_result(long duration);
};

class system_ping_tester final : public ping_tester_base {
public:
    test_result ping_host(const std::string& host, int timeout_ms) override;
    [[nodiscard]] ping_implementation get_implementation_type() const override {
        return ping_implementation::system_ping;
    }

private:
    static std::string build_ping_command(const std::string& host, int timeout_ms);

    static bool parse_ping_output(const std::string& output);
};

class icmp_ping_tester final : public ping_tester_base {
public:
    icmp_ping_tester();
    ~icmp_ping_tester() override;

    test_result ping_host(const std::string& host, int timeout_ms) override;
    [[nodiscard]] ping_implementation get_implementation_type() const override {
        return ping_implementation::unprivileged_icmp;
    }

private:
    bool initialize_socket();

    static bool send_icmp_packet(int socket, const struct sockaddr* dest_addr, socklen_t addr_len);

    static bool wait_for_reply(int socket, int timeout_ms);
    void cleanup_socket();

    int icmp_socket_;
    bool socket_initialized_;
};

class raw_socket_ping_tester final : public ping_tester_base {
public:
    raw_socket_ping_tester();
    ~raw_socket_ping_tester() override;

    test_result ping_host(const std::string& host, int timeout_ms) override;
    [[nodiscard]] ping_implementation get_implementation_type() const override {
        return ping_implementation::raw_socket;
    }

private:
    enum class socket_family { ipv4, ipv6 };

    struct ping_context {
        int socket_fd = -1;
        socket_family family;
        uint16_t sequence = 1;
        uint16_t identifier;
        bool socket_initialized = false;
    };

    static bool initialize_socket(socket_family family, ping_context& ctx);

    static bool send_icmp_packet(const ping_context& ctx, const struct sockaddr* dest_addr, socklen_t addr_len);

    static bool wait_for_reply(const ping_context& ctx, int timeout_ms);

    static void cleanup_socket(ping_context& ctx);

    static socket_family determine_address_family(const std::string& host);
    static uint16_t calculate_checksum(const void* data, size_t len);
    static bool resolve_hostname(const std::string& host, socket_family family, struct sockaddr_storage& addr, socklen_t& addr_len);

    ping_context ipv4_ctx_;
    ping_context ipv6_ctx_;
};

class auto_fallback_ping_tester final : public ping_tester_base {
public:
    auto_fallback_ping_tester();
    ~auto_fallback_ping_tester() override = default;

    test_result ping_host(const std::string& host, int timeout_ms) override;
    [[nodiscard]] ping_implementation get_implementation_type() const override {
        return current_implementation_;
    }

private:
    std::vector<std::unique_ptr<ping_tester_base>> implementations_;
    ping_implementation current_implementation_ = ping_implementation::system_ping;
};

class ping_tester_factory {
public:
    static std::unique_ptr<ping_tester_base> create_auto_fallback();

private:
    static std::unique_ptr<ping_tester_base> create(ping_implementation impl);
};