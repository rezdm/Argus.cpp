#include "web_server.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>

#include "../monitoring/monitor_config.h"

web_server::web_server(monitor_config config, const std::map<std::string, std::shared_ptr<monitor_state>>& monitors, std::shared_ptr<thread_pool> pool)
    : config_(std::move(config)),
      monitors_(monitors),
      thread_pool_(std::move(pool)),
      json_status_cached_(false),
      base_url_(config_.get_base_url()),
      cache_duration_(std::chrono::seconds(config_.get_cache_duration_seconds())) {
  // Initialize cached config name with fallback
  try {
    cached_config_name_ = config_.get_name().empty() ? "Argus Monitor" : config_.get_name();
  } catch (...) {
    cached_config_name_ = "Argus Monitor";
    spdlog::warn("Failed to access config name, using default");
  }

  // Generate static HTML page once
  generate_static_html_page();

  server_ = std::make_unique<httplib::Server>();

  // Configure server for better concurrency
  if (thread_pool_) {
    // httplib uses its own thread pool, we can set the read and write timeouts
    server_->set_read_timeout(30, 0);   // 30 seconds
    server_->set_write_timeout(30, 0);  // 30 seconds
    spdlog::debug("Web server configured with shared thread pool support ({} threads)", thread_pool_->thread_count());
  }

  // Set up route handlers
  server_->Get(base_url_ + "/web", [this](const httplib::Request& req, httplib::Response& res) { handle_status_request(req, res); });

  server_->Get(base_url_ + "/status", [this](const httplib::Request& req, httplib::Response& res) { handle_api_status_request(req, res); });

  // Parse listen address (support IPv4, IPv6, and hostnames)
  std::string host;
  int port;

  // Handle IPv6 address format [::1]:8080 or plain port number
  if ('[' == config_.get_listen().front()) {
    // IPv6 address in brackets format: [::1]:8080
    if (const size_t close_bracket = config_.get_listen().find(']'); std::string::npos != close_bracket) {
      host = config_.get_listen().substr(1, close_bracket - 1);  // Extract address without brackets
      if (const size_t colon_pos = config_.get_listen().find(':', close_bracket); std::string::npos != colon_pos) {
        port = std::stoi(config_.get_listen().substr(colon_pos + 1));
      } else {
        throw std::invalid_argument("Invalid IPv6 listen format: " + config_.get_listen());
      }
    } else {
      throw std::invalid_argument("Invalid IPv6 listen format: " + config_.get_listen());
    }
  } else if (const size_t last_colon = config_.get_listen().rfind(':'); std::string::npos != last_colon) {
    // IPv4 address or hostname format: 127.0.0.1:8080 or hostname:8080
    // Use rfind to get the last colon (for IPv4 or hostname with port)
    host = config_.get_listen().substr(0, last_colon);
    port = std::stoi(config_.get_listen().substr(last_colon + 1));

    // Check if this might be a bare IPv6 address without brackets and port
    if (std::string::npos != host.find(':') && std::string::npos != config_.get_listen().find("::")) {
      // This looks like a bare IPv6 address, treat the whole string as host
      host = config_.get_listen();
      port = 8080;  // Default port for IPv6 without explicit port
    }
  } else {
    // Just a port number
    host = "localhost";
    port = std::stoi(config_.get_listen());
  }

  // Start server in a separate thread
  server_thread_ = std::thread([this, host, port]() {
    spdlog::info("Argus web server starting on {}:{}", host, port);
    if (!server_->listen(host, port)) {
      spdlog::error("Failed to start web server on {}:{}", host, port);
    }
  });

  // Give the server a moment to start
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  spdlog::info("Argus web server started on {}", config_.get_listen());
}

web_server::~web_server() { stop(); }

void web_server::stop() {
  if (server_) {
    server_->stop();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
    spdlog::info("Web server stopped");
  }
}

void web_server::reload_html_template() {
  try {
    spdlog::info("Reloading HTML template from: {}", config_.get_html_template().value_or("(not configured)"));
    generate_static_html_page();
    spdlog::info("HTML template reloaded successfully");
  } catch (const std::exception& e) {
    spdlog::error("Failed to reload HTML template: {}", e.what());
  }
}

void web_server::handle_status_request(const httplib::Request& req, httplib::Response& res) const {
  spdlog::debug("HTTP request from {}: {} {}", req.remote_addr, req.method, req.path);

  res.set_content(static_html_page_, "text/html; charset=UTF-8");

  spdlog::trace("Served status page to {} ({} bytes)", req.remote_addr, static_html_page_.length());
}

void web_server::handle_api_status_request(const httplib::Request& req, httplib::Response& res) const {
  spdlog::debug("API request from {}: {} {}", req.remote_addr, req.method, req.path);

  const std::string response = generate_json_status();

  res.set_content(response, "application/json; charset=UTF-8");
  res.set_header("Access-Control-Allow-Origin", "*");

  spdlog::trace("Served JSON status to {} ({} bytes)", req.remote_addr, response.length());
}

void web_server::generate_static_html_page() {
  // HTML template is required - no fallback to inline generation
  if (!config_.get_html_template().has_value() || config_.get_html_template()->empty()) {
    throw std::runtime_error("html_template configuration is required - no default template available");
  }

  try {
    static_html_page_ = load_html_template_from_file(*config_.get_html_template());
    spdlog::info("Loaded static HTML template: {} ({} bytes)", *config_.get_html_template(), static_html_page_.length());
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to load required HTML template '" + *config_.get_html_template() + "': " + e.what());
  }
}

std::string web_server::generate_json_status() const {
  // Return cached JSON if available and valid
  if (json_status_cached_ && is_json_cache_valid()) {
    return cached_json_status_;
  }

  using json = nlohmann::json;

  try {
    json response;
    response["name"] = cached_config_name_;
    response["timestamp"] = format_timestamp(std::chrono::system_clock::now());
    response["groups"] = json::array();

    // Group monitors by group name
    std::map<std::string, std::vector<std::shared_ptr<monitor_state>>> grouped_monitors;
    for (const auto& state : monitors_ | std::views::values) {
      if (state) {  // Safety check for null pointer
        grouped_monitors[state->get_group_name()].push_back(state);
      }
    }

    // Sort groups and monitors
    std::vector<std::pair<std::string, std::vector<std::shared_ptr<monitor_state>>>> sorted_groups(grouped_monitors.begin(), grouped_monitors.end());

    // Sort groups by group sort order
    std::ranges::sort(sorted_groups, [](const auto& a, const auto& b) {
      if (a.second.empty() || b.second.empty()) return false;
      if (!a.second[0] || !b.second[0]) return false;
      return a.second[0]->get_group().get_sort() < b.second[0]->get_group().get_sort();
    });

    for (auto& [group_name, states] : sorted_groups) {
      // Sort monitors within group by destination sort order
      std::ranges::sort(states, [](const auto& a, const auto& b) {
        if (!a || !b) return false;
        return a->get_destination().get_sort() < b->get_destination().get_sort();
      });

      json group_obj;
      group_obj["name"] = group_name.empty() ? "Unknown Group" : group_name;
      group_obj["monitors"] = json::array();

      for (const auto& state : states) {
        if (!state) continue;  // Skip null states

        try {
          const std::string status_text = to_string(state->get_current_status());
          const auto* last_result = state->get_last_result();
          const std::string last_check = last_result ? format_timestamp(last_result->get_timestamp()) : "Never";
          const std::string response_time = last_result ? std::to_string(last_result->get_duration_ms()) + "ms" : "N/A";
          const double uptime_percent = state->get_uptime_percentage();
          const std::string test_details = state->get_test_description();
          const std::string host = state->get_destination().get_test().get_host().value_or("N/A");
          const std::string service_name = state->get_destination().get_name().empty() ? "Unknown Service" : state->get_destination().get_name();

          json monitor_obj;
          monitor_obj["service"] = service_name;
          monitor_obj["host"] = host;
          monitor_obj["status"] = status_text;
          monitor_obj["response_time"] = response_time;
          monitor_obj["response_time_ms"] = last_result ? last_result->get_duration_ms() : -1;
          monitor_obj["uptime_percent"] = uptime_percent;
          monitor_obj["last_check"] = last_check;
          monitor_obj["details"] = test_details;

          group_obj["monitors"].push_back(monitor_obj);
        } catch (const std::exception& e) {
          spdlog::error("Error serializing monitor state: {}", e.what());
          json error_obj;
          error_obj["error"] = "Error serializing monitor data";
          group_obj["monitors"].push_back(error_obj);
        }
      }

      response["groups"].push_back(group_obj);
    }

    // Cache the generated JSON
    cached_json_status_ = response.dump(2);  // Pretty print with 2-space indentation
    json_status_cached_ = true;
    last_cache_time_ = std::chrono::steady_clock::now();
    return cached_json_status_;
  } catch (const std::exception& e) {
    spdlog::error("Error generating JSON status: {}", e.what());
    json error_response;
    error_response["error"] = "Error generating monitor data";
    return error_response.dump(2);  // Don't cache error responses
  }
}

std::string web_server::load_html_template_from_file(const std::string& template_path) {
  std::ifstream file(template_path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open HTML template file: " + template_path);
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();

  if (buffer.str().empty()) {
    throw std::runtime_error("HTML template file is empty: " + template_path);
  }

  return buffer.str();
}

bool web_server::is_json_cache_valid() const {
  // If caching is disabled (duration = 0), always return false
  if (0 == cache_duration_.count()) {
    return false;
  }

  if (!json_status_cached_) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_cache_time_);

  return elapsed < cache_duration_;
}

void web_server::invalidate_json_cache() const {
  json_status_cached_ = false;
  cached_json_status_.clear();
}

std::string web_server::get_status_class(const monitor_status status) {
  switch (status) {
    case monitor_status::pending:
      return "status-pending";
    case monitor_status::ok:
      return "status-ok";
    case monitor_status::warning:
      return "status-warning";
    case monitor_status::failure:
      return "status-error";
    default:
      return "status-pending";
  }
}

std::string web_server::format_timestamp(const std::chrono::system_clock::time_point& timestamp) {
  const auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::ostringstream oss;
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return oss.str();
}
