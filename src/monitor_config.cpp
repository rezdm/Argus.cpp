#include "monitor_config.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

destination::destination(int sort, const std::string& name, int timeout, int warning, int failure, int reset, int interval, int history, test_config test)
    : sort(sort), name(name), timeout(timeout), warning(warning), failure(failure), reset(reset), interval(interval), history(history), test(std::move(test)) {
    
    if (this->name.empty()) {
        throw std::invalid_argument("Destination name cannot be empty");
    }
    if (timeout <= 0) {
        throw std::invalid_argument("Timeout must be positive");
    }
    if (warning <= 0 || failure <= 0) {
        throw std::invalid_argument("Warning and failure thresholds must be positive");
    }
}

monitor_config monitor_config::load_config(const std::string& config_path) {
    return monitor_config_loader::load_config(config_path);
}

monitor_config monitor_config_loader::load_config(const std::string& config_path) {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path);
    }

    json root;
    config_file >> root;

    monitor_config config;
    config.name = root["name"].get<std::string>();
    config.listen = root["listen"].get<std::string>();

    if (root.contains("log_file")) {
        config.log_file = root["log_file"].get<std::string>();
    }

    if (root.contains("ping_implementation")) {
        config.ping_impl = parse_ping_implementation(root["ping_implementation"].get<std::string>());
    }

    if (root.contains("cache_duration_seconds")) {
        config.cache_duration_seconds = root["cache_duration_seconds"].get<int>();
        if (config.cache_duration_seconds < 0) {
            throw std::invalid_argument("cache_duration_seconds must be non-negative (0 = no caching)");
        }
    }

    for (const auto& monitors_node = root["monitors"]; const auto& monitor_node : monitors_node) {
        config.monitors.push_back(parse_group(monitor_node));
    }

    // Sort groups by sort order
    std::ranges::sort(config.monitors, [](const group& a, const group& b) { return a.sort < b.sort; });

    return config;
}

test_config monitor_config_loader::parse_test_config(const json& test_node) {
    test_config config;
    
    config.test_method_type = parse_test_method(test_node["method"].get<std::string>());
    
    if (test_node.contains("protocol")) {
        config.protocol_type = parse_protocol(test_node["protocol"].get<std::string>());
    }
    
    if (test_node.contains("port")) {
        config.port = test_node["port"].get<int>();
    }
    
    if (test_node.contains("url")) {
        config.url = test_node["url"].get<std::string>();
    }
    
    if (test_node.contains("proxy")) {
        config.proxy = test_node["proxy"].get<std::string>();
    }
    
    if (test_node.contains("host")) {
        config.host = test_node["host"].get<std::string>();
    }
    
    return config;
}

destination monitor_config_loader::parse_destination(const json& dest_node) {
    auto test_config = parse_test_config(dest_node["test"]);
    
    return {
        dest_node["sort"].get<int>(),
        dest_node["name"].get<std::string>(),
        dest_node["timeout"].get<int>(),
        dest_node["warning"].get<int>(),
        dest_node["failure"].get<int>(),
        dest_node["reset"].get<int>(),
        dest_node["interval"].get<int>(),
        dest_node["history"].get<int>(),
        std::move(test_config)
    };
}

group monitor_config_loader::parse_group(const json& group_node) {
    group grp;
    grp.sort = group_node["sort"].get<int>();
    grp.group_name = group_node["group"].get<std::string>();

    for (const auto& destinations_node = group_node["destinations"]; const auto& dest_node : destinations_node) {
        grp.destinations.push_back(parse_destination(dest_node));
    }

    // Sort destinations by sort order
    std::ranges::sort(grp.destinations, [](const destination& a, const destination& b) { return a.sort < b.sort; });

    return grp;
}

std::string to_string(const test_method method) {
    switch (method) {
        case test_method::ping: return "ping";
        case test_method::connect: return "connect";
        case test_method::url: return "url";
        default: throw std::invalid_argument("Unknown test method");
    }
}

std::string to_string(const protocol proto) {
    switch (proto) {
        case protocol::tcp: return "tcp";
        case protocol::udp: return "udp";
        default: throw std::invalid_argument("Unknown protocol");
    }
}

std::string to_string(const monitor_status status) {
    switch (status) {
        case monitor_status::ok: return "OK";
        case monitor_status::warning: return "WARNING";
        case monitor_status::failure: return "FAILURE";
        default: throw std::invalid_argument("Unknown monitor status");
    }
}

std::string to_string(const ping_implementation impl) {
    switch (impl) {
        case ping_implementation::system_ping: return "system_ping";
        case ping_implementation::unprivileged_icmp: return "unprivileged_icmp";
        default: throw std::invalid_argument("Unknown ping implementation");
    }
}

test_method parse_test_method(const std::string& str) {
    std::string lower_str = str;
    std::ranges::transform(lower_str, lower_str.begin(), ::tolower);
    
    if (lower_str == "ping") return test_method::ping;
    if (lower_str == "connect") return test_method::connect;
    if (lower_str == "url") return test_method::url;
    
    throw std::invalid_argument("Unknown test method: " + str);
}

protocol parse_protocol(const std::string& str) {
    std::string lower_str = str;
    std::ranges::transform(lower_str, lower_str.begin(), ::tolower);

    if (lower_str == "tcp") return protocol::tcp;
    if (lower_str == "udp") return protocol::udp;

    throw std::invalid_argument("Unknown protocol: " + str);
}

ping_implementation parse_ping_implementation(const std::string& str) {
    std::string lower_str = str;
    std::ranges::transform(lower_str, lower_str.begin(), ::tolower);

    if (lower_str == "system_ping") return ping_implementation::system_ping;
    if (lower_str == "unprivileged_icmp") return ping_implementation::unprivileged_icmp;

    throw std::invalid_argument("Unknown ping implementation: " + str);
}