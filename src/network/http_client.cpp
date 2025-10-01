#include "http_client.h"

#include "../utils/test_result.h"

// Platform-specific exception handling for FreeBSD
#ifdef __FreeBSD__
#include <exception>
#endif

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <optional>

test_result http_client_base::create_error_result(const std::string& error_msg, const long duration) {
  return test_result{false, duration, std::chrono::system_clock::now(), error_msg};
}

test_result http_client_base::create_success_result(const long duration) { return test_result{true, duration, std::chrono::system_clock::now(), std::nullopt}; }

// HTTP Client Implementation
test_result http_client::perform_request(const std::string& host, const std::string& path, const int timeout_ms, const std::string& proxy) {
  const auto start_time = std::chrono::steady_clock::now();

  try {
    std::string actual_host = host;
    const int port = extract_port_from_host(actual_host, 80);

    httplib::Client client(actual_host, port);

    // Set timeouts
    client.set_connection_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
    client.set_read_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
    client.set_write_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);

    // Set headers
    const httplib::Headers headers = {{"User-Agent", "Argus/1.0 (Network Monitor)"}, {"Accept", "*/*"}, {"Connection", "close"}};

    // TODO: Implement proxy support if needed
    if (!proxy.empty()) {
      spdlog::debug("Proxy support not implemented yet: {}", proxy);
    }

    auto result = client.Get(path, headers);

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (!result) {
      spdlog::debug("HTTP request failed for {}:{}{}: Connection error", actual_host, port, path);
      return create_error_result("Connection failed", duration);
    }

    // Consider 2xx status codes as success
    if (result->status >= 200 && result->status < 300) {
      return create_success_result(duration);
    } else {
      spdlog::debug("HTTP request failed for {}:{}{}: HTTP {}", actual_host, port, path, result->status);
      return create_error_result("HTTP " + std::to_string(result->status), duration);
    }

  } catch (const std::exception& e) {
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    return create_error_result(e.what(), duration);
  }
}

int http_client::extract_port_from_host(std::string& host, const int default_port) {
  const size_t port_pos = host.find(':');
  if (std::string::npos != port_pos) {
    const int port = std::stoi(host.substr(port_pos + 1));
    host = host.substr(0, port_pos);
    return port;
  }
  return default_port;
}

// HTTPS Client Implementation
https_client::https_client() : enable_cert_verification_(false) {
  // For now, disable certificate verification for monitoring purposes
  // This could be made configurable in the future
}

test_result https_client::perform_request(const std::string& host, const std::string& path, const int timeout_ms, const std::string& proxy) {
  const auto start_time = std::chrono::steady_clock::now();

  try {
    std::string actual_host = host;
    const int port = extract_port_from_host(actual_host, 443);

    httplib::SSLClient ssl_client(actual_host, port);

    // Set timeouts
    ssl_client.set_connection_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
    ssl_client.set_read_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
    ssl_client.set_write_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);

    // Configure SSL verification
    ssl_client.enable_server_certificate_verification(enable_cert_verification_);

    // Set headers
    const httplib::Headers headers = {{"User-Agent", "Argus/1.0 (Network Monitor)"}, {"Accept", "*/*"}, {"Connection", "close"}};

    // TODO: Implement proxy support if needed
    if (!proxy.empty()) {
      spdlog::debug("Proxy support not implemented yet: {}", proxy);
    }

    auto result = ssl_client.Get(path, headers);

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (!result) {
      spdlog::debug("HTTPS request failed for {}:{}{}: Connection error", actual_host, port, path);
      return create_error_result("SSL connection failed", duration);
    }

    // Consider 2xx status codes as success
    if (result->status >= 200 && result->status < 300) {
      return create_success_result(duration);
    } else {
      spdlog::debug("HTTPS request failed for {}:{}{}: HTTP {}", actual_host, port, path, result->status);
      return create_error_result("HTTP " + std::to_string(result->status), duration);
    }

  } catch (const std::exception& e) {
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    return create_error_result(e.what(), duration);
  }
}

int https_client::extract_port_from_host(std::string& host, const int default_port) {
  const size_t port_pos = host.find(':');
  if (std::string::npos != port_pos) {
    const int port = std::stoi(host.substr(port_pos + 1));
    host = host.substr(0, port_pos);
    return port;
  }
  return default_port;
}

// Factory Implementation
std::unique_ptr<http_client_base> http_client_factory::create(const std::string& scheme) {
  if ("https" == scheme) {
    return std::make_unique<https_client>();
  } else if ("http" == scheme) {
    return std::make_unique<http_client>();
  }
  throw std::invalid_argument("Unsupported HTTP scheme: " + scheme);
}