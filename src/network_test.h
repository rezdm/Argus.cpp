#pragma once

#include "types.h"
#include <string>

class network_test {
public:
    virtual ~network_test() = default;
    [[nodiscard]] virtual test_result execute(const test_config& config, int timeout_ms) const = 0;
    [[nodiscard]] virtual std::string get_description(const test_config& config) const = 0;
    virtual void validate_config(const test_config& config) const = 0;
};