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

bool test_config::is_valid() const {
  switch (test_method_type_) {
    case test_method::ping:
      return host_.has_value();
    case test_method::connect:
      return host_.has_value() && port_ > 0 && protocol_type_.has_value();
    case test_method::url:
      return url_.has_value();
  }
  return false;
}

std::string test_config::get_validation_error() const {
  if (is_valid()) return "";

  switch (test_method_type_) {
    case test_method::ping:
      if (!host_.has_value()) return "Ping test requires a host";
      break;
    case test_method::connect:
      if (!host_.has_value()) return "Connect test requires a host";
      if (port_ <= 0) return "Connect test requires a valid port (1-65535)";
      if (!protocol_type_.has_value()) return "Connect test requires a protocol";
      break;
    case test_method::url:
      if (!url_.has_value()) return "URL test requires a URL";
      break;
  }
  return "Unknown validation error";
}