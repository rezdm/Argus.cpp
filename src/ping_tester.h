#pragma once

#include "types.h"
#include <string>
#include <memory>

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

class ping_tester_factory {
public:
    static std::unique_ptr<ping_tester_base> create(ping_implementation impl);
};