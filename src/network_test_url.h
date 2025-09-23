#pragma once

#include "network_test.h"

class network_test_url final : public network_test {
public:
    [[nodiscard]] test_result execute(const test_config& config, int timeout_ms) const override;
    [[nodiscard]] std::string get_description(const test_config& config) const override;
    void validate_config(const test_config& config) const override;

private:
    static bool perform_http_request(const std::string& url, const std::string& proxy, int timeout_ms);
};