#pragma once

#include "network_test.h"

class network_test_url : public network_test {
public:
    test_result execute(const test_config& config, int timeout_ms) override;
    std::string get_description(const test_config& config) override;
    void validate_config(const test_config& config) override;

private:
    bool perform_http_request(const std::string& url, const std::string& proxy, int timeout_ms);
};