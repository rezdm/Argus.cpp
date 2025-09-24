#pragma once

#include "types.h"
#include <string>
#include <memory>

class connection_tester_base {
public:
    virtual ~connection_tester_base() = default;
    virtual test_result test_connection(const std::string& host, int port, int timeout_ms) = 0;
    [[nodiscard]] virtual protocol get_protocol_type() const;

protected:
    static test_result create_error_result(const std::string& error_msg, long duration = 0);
    static test_result create_success_result(long duration);
    static bool resolve_address(const std::string& host, int port, int socket_type, struct addrinfo** result);
};

class tcp_connection_tester final : public connection_tester_base {
public:
    test_result test_connection(const std::string& host, int port, int timeout_ms) override;
    [[nodiscard]] protocol get_protocol_type() const override { return protocol::tcp; }

private:
    bool test_single_address(const addrinfo* addr_info, int timeout_ms);
};

class udp_connection_tester final : public connection_tester_base {
public:
    test_result test_connection(const std::string& host, int port, int timeout_ms) override;
    [[nodiscard]] protocol get_protocol_type() const override { return protocol::udp; }

private:
    bool test_single_address(const addrinfo* addr_info, int timeout_ms);
};

class connection_tester_factory {
public:
    static std::unique_ptr<connection_tester_base> create(protocol proto);
};