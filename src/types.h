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

enum class ping_implementation {
    system_ping,
    unprivileged_icmp
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
    
    destination(int sort, const std::string& name, int timeout, int warning, int failure, int reset, int interval, int history, test_config test);
};

struct group {
    int sort;
    std::string group_name;
    std::vector<destination> destinations;
};

struct monitor_config {
    std::string name;
    std::string listen;
    std::optional<std::string> log_file;
    ping_implementation ping_impl = ping_implementation::system_ping;
    int cache_duration_seconds = 30; // Default 30 seconds, 0 = no caching
    std::optional<std::string> html_template; // Path to custom HTML template file
    size_t thread_pool_size = 0; // Default 0 = use hardware concurrency
    std::vector<group> monitors;

    static monitor_config load_config(const std::string& config_path);
};

struct test_result {
    bool success;
    long duration_ms;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> error;
    
    test_result(bool success, long duration_ms, std::chrono::system_clock::time_point timestamp, const std::optional<std::string>& error = std::nullopt);
};

// String conversion functions
std::string to_string(test_method method);
std::string to_string(protocol proto);
std::string to_string(monitor_status status);
std::string to_string(ping_implementation impl);
test_method parse_test_method(const std::string& str);
protocol parse_protocol(const std::string& str);
ping_implementation parse_ping_implementation(const std::string& str);