#pragma once

#include "network_test.h"

class network_test_ping : public network_test {
public:
    test_result execute(const test_config& config, int timeout_ms) override;
    std::string get_description(const test_config& config) override;
    void validate_config(const test_config& config) override;

private:
    bool ping_host(const std::string& host, int timeout_ms);
};