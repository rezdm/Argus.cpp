#include "test_factory.h"

#include <ranges>

#include "network_test_ping.h"
#include "network_test_connect.h"
#include "network_test_url.h"
#include <stdexcept>

std::unordered_map<test_method, std::shared_ptr<network_test>> test_factory::test_implementations_;
bool test_factory::initialized_ = false;

void test_factory::initialize_default_tests() {
    if (initialized_) return;
    
    test_implementations_[test_method::ping] = std::make_shared<network_test_ping>();
    test_implementations_[test_method::connect] = std::make_shared<network_test_connect>();
    test_implementations_[test_method::url] = std::make_shared<network_test_url>();
    
    initialized_ = true;
}

std::shared_ptr<network_test> test_factory::get_test(const test_method method) {
    initialize_default_tests();

    const auto it = test_implementations_.find(method);
    if (it == test_implementations_.end()) {
        throw std::invalid_argument("Unsupported test method: " + to_string(method));
    }
    
    return it->second;
}

void test_factory::register_test(const test_method method, const std::shared_ptr<network_test> &implementation) {
    if (!implementation) {
        throw std::invalid_argument("Implementation cannot be null");
    }
    test_implementations_[method] = implementation;
}

std::set<test_method> test_factory::get_supported_methods() {
    initialize_default_tests();
    
    std::set<test_method> methods;
    for (const auto &key: test_implementations_ | std::views::keys) {
        methods.insert(key);
    }
    return methods;
}

std::string test_factory::validate_and_describe(const test_config& config) {
    const auto test = get_test(config.get_test_method());
    test->validate_config(config);
    return test->get_description(config);
}