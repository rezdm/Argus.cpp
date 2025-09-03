#pragma once

#include "types.h"
#include "network_test.h"
#include <memory>
#include <unordered_map>
#include <set>

class test_factory {
public:
    static std::shared_ptr<network_test> get_test(test_method method);
    static void register_test(test_method method, std::shared_ptr<network_test> implementation);
    static std::set<test_method> get_supported_methods();
    static std::string validate_and_describe(const test_config& config);

private:
    static std::unordered_map<test_method, std::shared_ptr<network_test>> test_implementations_;
    static void initialize_default_tests();
    static bool initialized_;
};