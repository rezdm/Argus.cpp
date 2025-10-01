#include "test_config.h"

#include <stdexcept>

void test_config::set_test_method(const test_method method) { test_method_type_ = method; }

void test_config::set_protocol(protocol proto) { protocol_type_ = proto; }

void test_config::clear_protocol() { protocol_type_.reset(); }

void test_config::set_port(const int port_val) {
  if (port_val < -1 || port_val > 65535) {
    throw std::invalid_argument("Port must be -1 (unset) or between 0-65535");
  }
  port_ = port_val;
}

void test_config::set_url(const std::string& url_val) {
  if (url_val.empty()) {
    throw std::invalid_argument("URL cannot be empty");
  }
  url_ = url_val;
}

void test_config::clear_url() { url_.reset(); }

void test_config::set_proxy(const std::string& proxy_val) { proxy_ = proxy_val; }

void test_config::clear_proxy() { proxy_.reset(); }

void test_config::set_host(const std::string& host_val) {
  if (host_val.empty()) {
    throw std::invalid_argument("Host cannot be empty");
  }
  host_ = host_val;
}

void test_config::clear_host() { host_.reset(); }

std::unique_ptr<test_config_validator> test_config::get_validator() const {
  return test_config_validator_factory::create(test_method_type_);
}

bool test_config::is_valid() const {
  const auto validator = get_validator();
  return validator->is_valid(*this);
}

std::string test_config::get_validation_error() const {
  const auto validator = get_validator();
  return validator->get_validation_error(*this);
}

// Ping Test Validator Implementation
bool ping_test_validator::is_valid(const test_config& config) const {
  return config.get_host().has_value();
}

std::string ping_test_validator::get_validation_error(const test_config& config) const {
  if (is_valid(config)) return "";
  if (!config.get_host().has_value()) return "Ping test requires a host";
  return "Unknown validation error";
}

// Connect Test Validator Implementation
bool connect_test_validator::is_valid(const test_config& config) const {
  return config.get_host().has_value() &&
         config.get_port() > 0 &&
         config.get_protocol().has_value();
}

std::string connect_test_validator::get_validation_error(const test_config& config) const {
  if (is_valid(config)) return "";
  if (!config.get_host().has_value()) return "Connect test requires a host";
  if (config.get_port() <= 0) return "Connect test requires a valid port (1-65535)";
  if (!config.get_protocol().has_value()) return "Connect test requires a protocol";
  return "Unknown validation error";
}

// URL Test Validator Implementation
bool url_test_validator::is_valid(const test_config& config) const {
  return config.get_url().has_value();
}

std::string url_test_validator::get_validation_error(const test_config& config) const {
  if (is_valid(config)) return "";
  if (!config.get_url().has_value()) return "URL test requires a URL";
  return "Unknown validation error";
}

// Factory Implementation
std::unique_ptr<test_config_validator> test_config_validator_factory::create(const test_method method) {
  switch (method) {
    case test_method::ping:
      return std::make_unique<ping_test_validator>();
    case test_method::connect:
      return std::make_unique<connect_test_validator>();
    case test_method::url:
      return std::make_unique<url_test_validator>();
    default:
      throw std::invalid_argument("Unknown test method");
  }
}