#pragma once

#include "network_test.h"

class network_test_connect final : public network_test {
public:
    [[nodiscard]] test_result execute(const test_config& config, int timeout_ms) const override;
    [[nodiscard]] std::string get_description(const test_config& config) const override;
    void validate_config(const test_config& config) const override;

private:
    static bool test_tcp_connection(const std::string& host, int port, int timeout_ms);

    static bool test_udp_connection(const std::string& host, int port, int timeout_ms);
};