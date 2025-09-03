#pragma once

#include "types.h"
#include <string>

class network_test {
public:
    virtual ~network_test() = default;
    virtual test_result execute(const test_config& config, int timeout_ms) = 0;
    virtual std::string get_description(const test_config& config) = 0;
    virtual void validate_config(const test_config& config) = 0;
};