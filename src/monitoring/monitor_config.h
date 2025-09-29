#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "destination.h"
#include "group.h"
#include "monitor_config_types.h"
#include "test_config.h"

// Utility functions for enums
test_method parse_test_method(const std::string& str);
protocol parse_protocol(const std::string& str);
std::string to_string(test_method method);
std::string to_string(protocol proto);
std::string to_string(monitor_status status);

class monitor_config_loader {
 public:
  static monitor_config load_config(const std::string& config_path);

 private:
  static test_config parse_test_config(const nlohmann::json& test_node);
  static destination parse_destination(const nlohmann::json& dest_node);
  static group parse_group(const nlohmann::json& group_node);
};