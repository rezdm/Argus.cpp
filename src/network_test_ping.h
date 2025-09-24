#pragma once

#include "network_test.h"

class network_test_ping final : public network_test {
public:
    [[nodiscard]] test_result execute(const test_config& config, int timeout_ms) const override;
    [[nodiscard]] std::string get_description(const test_config& config) const override;
    void validate_config(const test_config& config) const override;

    // Static configuration for ping implementation
    static void set_ping_implementation(ping_implementation impl);
    static ping_implementation get_ping_implementation();

private:
    static ping_implementation ping_impl_;
};