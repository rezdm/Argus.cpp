#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../core/constants.h"
#include "group.h"

class monitor_config {
 private:
  std::string name_;
  std::string listen_;
  std::optional<std::string> log_file_;
  int cache_duration_seconds_;
  std::optional<std::string> html_template_;
  std::string base_url_;
  size_t thread_pool_size_;
  std::vector<group> monitors_;

 public:
  // Constructors
  monitor_config() : cache_duration_seconds_(argus::constants::DEFAULT_CACHE_DURATION_SECONDS), base_url_(argus::constants::DEFAULT_BASE_URL), thread_pool_size_(argus::constants::DEFAULT_THREAD_POOL_SIZE) {}

  monitor_config(const std::string& name_val, const std::string& listen_val);

  // Getters
  [[nodiscard]] const std::string& get_name() const { return name_; }
  [[nodiscard]] const std::string& get_listen() const { return listen_; }
  [[nodiscard]] const std::optional<std::string>& get_log_file() const { return log_file_; }
  [[nodiscard]] int get_cache_duration_seconds() const { return cache_duration_seconds_; }
  [[nodiscard]] const std::optional<std::string>& get_html_template() const { return html_template_; }
  [[nodiscard]] const std::string& get_base_url() const { return base_url_; }
  [[nodiscard]] size_t get_thread_pool_size() const { return thread_pool_size_; }
  [[nodiscard]] const std::vector<group>& get_monitors() const { return monitors_; }
  [[nodiscard]] size_t get_monitor_count() const { return monitors_.size(); }

  // Setters with validation
  void set_name(const std::string& name_val);
  void set_listen(const std::string& listen_val);
  void set_log_file(const std::string& log_file_val);
  void clear_log_file();
  void set_cache_duration_seconds(int duration);
  void set_html_template(const std::string& template_val);
  void clear_html_template();
  void set_base_url(const std::string& base_url_val);
  void set_thread_pool_size(size_t size);
  void set_monitors(const std::vector<group>& monitors_val);
  void add_monitor_group(const group& monitor_group);
  void clear_monitors();

  // Validation methods
  [[nodiscard]] bool is_valid() const;
  [[nodiscard]] std::string get_validation_error() const;

  // Static factory method (keeping for backward compatibility)
  static monitor_config load_config(const std::string& config_path);

 private:
  void validate_parameters() const;
};