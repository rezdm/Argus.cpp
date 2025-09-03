#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>

enum class test_method {
    ping,
    connect,
    url
};

enum class protocol {
    tcp,
    udp
};

enum class monitor_status {
    ok,
    warning,
    failure
};

struct test_config {
    test_method test_method_type;
    std::optional<protocol> protocol_type;
    int port = -1;
    std::optional<std::string> url;
    std::optional<std::string> proxy;
    std::optional<std::string> host;
};

struct destination {
    int sort;
    std::string name;
    int timeout;
    int warning;
    int failure;
    int reset;
    int interval;
    int history;
    test_config test;
    
    destination(int sort, std::string name, int timeout, int warning, 
                int failure, int reset, int interval, int history, test_config test);
};

struct group {
    int sort;
    std::string group_name;
    std::vector<destination> destinations;
};

struct monitor_config {
    std::string name;
    std::string listen;
    std::vector<group> monitors;
    
    static monitor_config load_config(const std::string& config_path);
};

struct test_result {
    bool success;
    long duration_ms;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> error;
    
    test_result(bool success, long duration_ms, 
                std::chrono::system_clock::time_point timestamp,
                std::optional<std::string> error = std::nullopt);
};

// String conversion functions
std::string to_string(test_method method);
std::string to_string(protocol proto);
std::string to_string(monitor_status status);
test_method parse_test_method(const std::string& str);
protocol parse_protocol(const std::string& str);