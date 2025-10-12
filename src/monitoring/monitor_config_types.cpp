#include "monitor_config_types.h"

#include <stdexcept>

#include "../core/constants.h"

monitor_config::monitor_config(const std::string& name_val, const std::string& listen_val)
    : name_(name_val),
      listen_(listen_val),
      cache_duration_seconds_(argus::constants::DEFAULT_CACHE_DURATION_SECONDS),
      base_url_(argus::constants::DEFAULT_BASE_URL),
      thread_pool_size_(argus::constants::DEFAULT_THREAD_POOL_SIZE) {
  validate_parameters();
}

void monitor_config::set_name(const std::string& name_val) {
  if (name_val.empty()) {
    throw std::invalid_argument("Monitor config name cannot be empty");
  }
  name_ = name_val;
}

void monitor_config::set_listen(const std::string& listen_val) {
  if (listen_val.empty()) {
    throw std::invalid_argument("Listen address cannot be empty");
  }
  listen_ = listen_val;
}

void monitor_config::set_log_file(const std::string& log_file_val) { log_file_ = log_file_val; }

void monitor_config::clear_log_file() { log_file_.reset(); }

void monitor_config::set_cache_duration_seconds(const int duration) {
  if (duration < 0) {
    throw std::invalid_argument("Cache duration cannot be negative");
  }
  cache_duration_seconds_ = duration;
}

void monitor_config::set_html_template(const std::string& template_val) { html_template_ = template_val; }

void monitor_config::clear_html_template() { html_template_.reset(); }

void monitor_config::set_base_url(const std::string& base_url_val) { base_url_ = base_url_val; }

void monitor_config::set_thread_pool_size(const size_t size) { thread_pool_size_ = size; }

void monitor_config::set_static_dir(const std::string& static_dir_val) { static_dir_ = static_dir_val; }

void monitor_config::clear_static_dir() { static_dir_.reset(); }

void monitor_config::set_pwa_path(const std::string& pwa_path_val) { pwa_path_ = pwa_path_val; }

void monitor_config::set_push_config(const push_notification_config& push_config_val) {
  if (!push_config_val.is_valid()) {
    throw std::invalid_argument("Invalid push notification config: " + push_config_val.get_validation_error());
  }
  push_config_ = push_config_val;
}

void monitor_config::set_monitors(const std::vector<group>& monitors_val) {
  for (const auto& monitor_group : monitors_val) {
    if (!monitor_group.is_valid()) {
      throw std::invalid_argument("Invalid monitor group: " + monitor_group.get_validation_error());
    }
  }
  monitors_ = monitors_val;
}

void monitor_config::add_monitor_group(const group& monitor_group) {
  if (!monitor_group.is_valid()) {
    throw std::invalid_argument("Cannot add invalid monitor group: " + monitor_group.get_validation_error());
  }
  monitors_.push_back(monitor_group);
}

void monitor_config::clear_monitors() { monitors_.clear(); }

bool monitor_config::is_valid() const {
  if (name_.empty() || listen_.empty()) return false;
  if (cache_duration_seconds_ < 0) return false;
  for (const auto& monitor_group : monitors_) {
    if (!monitor_group.is_valid()) return false;
  }
  return true;
}

std::string monitor_config::get_validation_error() const {
  if (name_.empty()) return "Monitor config name cannot be empty";
  if (listen_.empty()) return "Listen address cannot be empty";
  if (cache_duration_seconds_ < 0) return "Cache duration cannot be negative";
  for (size_t i = 0; i < monitors_.size(); ++i) {
    if (!monitors_[i].is_valid()) {
      return "Monitor group " + std::to_string(i) + " is invalid: " + monitors_[i].get_validation_error();
    }
  }
  return "";
}

void monitor_config::validate_parameters() const {
  if (!is_valid()) {
    throw std::invalid_argument("Invalid monitor config parameters: " + get_validation_error());
  }
}