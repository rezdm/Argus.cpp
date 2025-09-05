#pragma once

#include "types.h"
#include <string>
#include <nlohmann/json.hpp>

class monitor_config_loader {
public:
    static monitor_config load_config(const std::string& config_path);
private:
    static test_config parse_test_config(const nlohmann::json& test_node);
    static destination parse_destination(const nlohmann::json& dest_node);
    static group parse_group(const nlohmann::json& group_node);
};