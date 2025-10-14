#include "monitor_config.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

monitor_config monitor_config::load_config(const std::string& config_path) { return monitor_config_loader::load_config(config_path); }

monitor_config monitor_config_loader::load_config(const std::string& config_path) {
  std::ifstream config_file(config_path);
  if (!config_file.is_open()) {
    throw std::runtime_error("Cannot open config file: " + config_path);
  }

  json root;
  config_file >> root;

  monitor_config config;
  config.set_name(root["name"].get<std::string>());
  config.set_listen(root["listen"].get<std::string>());

  if (root.contains("log_file")) {
    config.set_log_file(root["log_file"].get<std::string>());
  }

  if (root.contains("cache_duration_seconds")) {
    config.set_cache_duration_seconds(root["cache_duration_seconds"].get<int>());
  }

  if (root.contains("html_template")) {
    config.set_html_template(root["html_template"].get<std::string>());
  }

  if (root.contains("base_url")) {
    config.set_base_url(root["base_url"].get<std::string>());
  }

  if (root.contains("thread_pool_size")) {
    config.set_thread_pool_size(root["thread_pool_size"].get<size_t>());
    if (config.get_thread_pool_size() > 1000) {
      spdlog::warn("Large thread pool size specified: {}. Consider using a smaller value for better resource management.", config.get_thread_pool_size());
    }
  }

  if (root.contains("static_dir")) {
    config.set_static_dir(root["static_dir"].get<std::string>());
  }

  if (root.contains("pwa_path")) {
    config.set_pwa_path(root["pwa_path"].get<std::string>());
  }

  if (root.contains("log_status_every_n")) {
    config.set_log_status_every_n(root["log_status_every_n"].get<int>());
  }

  if (root.contains("push_notifications")) {
    const auto& push_node = root["push_notifications"];
    push_notification_config push_config;
    push_config.enabled = push_node.value("enabled", false);

    if (push_config.enabled) {
      push_config.vapid_subject = push_node.value("vapid_subject", "");
      push_config.vapid_public_key = push_node.value("vapid_public_key", "");
      push_config.vapid_private_key = push_node.value("vapid_private_key", "");
      push_config.subscriptions_file = push_node.value("subscriptions_file", "push_subscriptions.json");

      // Default suppressions file to be in same directory as subscriptions file
      std::string default_suppressions_file = "push_suppressions.json";
      if (push_config.subscriptions_file.find('/') != std::string::npos) {
        // If subscriptions file has a path, use same directory for suppressions
        size_t last_slash = push_config.subscriptions_file.find_last_of('/');
        default_suppressions_file = push_config.subscriptions_file.substr(0, last_slash + 1) + "push_suppressions.json";
      }
      push_config.suppressions_file = push_node.value("suppressions_file", default_suppressions_file);

      if (!push_config.is_valid()) {
        spdlog::warn("Push notifications enabled but configuration is invalid: {}. Push notifications will be disabled.", push_config.get_validation_error());
        push_config.enabled = false;
      } else {
        spdlog::info("Push notifications enabled");
      }
    }

    config.set_push_config(push_config);
  }

  for (const auto& monitors_node = root["monitors"]; const auto& monitor_node : monitors_node) {
    config.add_monitor_group(parse_group(monitor_node));
  }

  // Sort groups by sort order
  auto monitors = config.get_monitors();
  std::ranges::sort(monitors, [](const group& a, const group& b) { return a.get_sort() < b.get_sort(); });
  config.set_monitors(monitors);

  return config;
}

test_config monitor_config_loader::parse_test_config(const json& test_node) {
  test_config config;

  config.set_test_method(parse_test_method(test_node["method"].get<std::string>()));

  if (test_node.contains("protocol")) {
    config.set_protocol(parse_protocol(test_node["protocol"].get<std::string>()));
  }

  if (test_node.contains("port")) {
    config.set_port(test_node["port"].get<int>());
  }

  if (test_node.contains("url")) {
    config.set_url(test_node["url"].get<std::string>());
  }

  if (test_node.contains("proxy")) {
    config.set_proxy(test_node["proxy"].get<std::string>());
  }

  if (test_node.contains("host")) {
    config.set_host(test_node["host"].get<std::string>());
  }

  if (test_node.contains("run")) {
    config.set_cmd_run(test_node["run"].get<std::string>());
  }

  if (test_node.contains("expect")) {
    config.set_cmd_expect(test_node["expect"].get<int>());
  }

  return config;
}

destination monitor_config_loader::parse_destination(const json& dest_node) {
  auto test_config = parse_test_config(dest_node["test"]);

  return {dest_node["sort"].get<int>(),  dest_node["name"].get<std::string>(), dest_node["timeout"].get<int>(), dest_node["warning"].get<int>(), dest_node["failure"].get<int>(),
          dest_node["reset"].get<int>(), dest_node["interval"].get<int>(),     dest_node["history"].get<int>(), std::move(test_config)};
}

group monitor_config_loader::parse_group(const json& group_node) {
  group grp;
  grp.set_sort(group_node["sort"].get<int>());
  grp.set_group_name(group_node["group"].get<std::string>());

  for (const auto& destinations_node = group_node["destinations"]; const auto& dest_node : destinations_node) {
    grp.add_destination(parse_destination(dest_node));
  }

  // Sort destinations by sort order
  auto destinations = grp.get_destinations();
  std::ranges::sort(destinations, [](const destination& a, const destination& b) { return a.get_sort() < b.get_sort(); });
  grp.set_destinations(destinations);

  return grp;
}

std::string to_string(const test_method method) {
  switch (method) {
    case test_method::ping:
      return "ping";
    case test_method::connect:
      return "connect";
    case test_method::url:
      return "url";
    case test_method::cmd:
      return "cmd";
    default:
      throw std::invalid_argument("Unknown test method");
  }
}

std::string to_string(const protocol proto) {
  switch (proto) {
    case protocol::tcp:
      return "tcp";
    case protocol::udp:
      return "udp";
    default:
      throw std::invalid_argument("Unknown protocol");
  }
}

std::string to_string(const monitor_status status) {
  switch (status) {
    case monitor_status::pending:
      return "PENDING";
    case monitor_status::ok:
      return "OK";
    case monitor_status::warning:
      return "WARNING";
    case monitor_status::failure:
      return "FAILURE";
    default:
      throw std::invalid_argument("Unknown monitor status");
  }
}

test_method parse_test_method(const std::string& str) {
  std::string lower_str = str;
  std::ranges::transform(lower_str, lower_str.begin(), ::tolower);

  if ("ping" == lower_str) return test_method::ping;
  if ("connect" == lower_str) return test_method::connect;
  if ("url" == lower_str) return test_method::url;
  if ("cmd" == lower_str) return test_method::cmd;

  throw std::invalid_argument("Unknown test method: " + str);
}

protocol parse_protocol(const std::string& str) {
  std::string lower_str = str;
  std::ranges::transform(lower_str, lower_str.begin(), ::tolower);

  if ("tcp" == lower_str) return protocol::tcp;
  if ("udp" == lower_str) return protocol::udp;

  throw std::invalid_argument("Unknown protocol: " + str);
}
