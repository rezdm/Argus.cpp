#pragma once

#include <memory>
#include <optional>
#include <string>

#include "../core/types.h"

// Forward declaration
class test_config_validator;

class test_config {
 private:
  test_method test_method_type_;
  std::optional<protocol> protocol_type_;
  int port_;
  std::optional<std::string> url_;
  std::optional<std::string> proxy_;
  std::optional<std::string> host_;

 public:
  // Constructors
  test_config() : test_method_type_(test_method::ping), port_(-1) {}

  explicit test_config(const test_method method) : test_method_type_(method), port_(-1) {}

  test_config(const test_method method, protocol proto, const int port_val) : test_method_type_(method), protocol_type_(proto), port_(port_val) {}

  test_config(const test_method method, const std::string& url_val) : test_method_type_(method), port_(-1), url_(url_val) {}

  // Getters
  [[nodiscard]] test_method get_test_method() const { return test_method_type_; }
  [[nodiscard]] const std::optional<protocol>& get_protocol() const { return protocol_type_; }
  [[nodiscard]] int get_port() const { return port_; }
  [[nodiscard]] const std::optional<std::string>& get_url() const { return url_; }
  [[nodiscard]] const std::optional<std::string>& get_proxy() const { return proxy_; }
  [[nodiscard]] const std::optional<std::string>& get_host() const { return host_; }

  // Setters with validation
  void set_test_method(test_method method);
  void set_protocol(protocol proto);
  void clear_protocol();
  void set_port(int port_val);
  void set_url(const std::string& url_val);
  void clear_url();
  void set_proxy(const std::string& proxy_val);
  void clear_proxy();
  void set_host(const std::string& host_val);
  void clear_host();

  // Validation methods
  [[nodiscard]] bool is_valid() const;
  [[nodiscard]] std::string get_validation_error() const;

 private:
  [[nodiscard]] std::unique_ptr<test_config_validator> get_validator() const;
};

// Validator Strategy Pattern
class test_config_validator {
 public:
  virtual ~test_config_validator() = default;
  [[nodiscard]] virtual bool is_valid(const test_config& config) const = 0;
  [[nodiscard]] virtual std::string get_validation_error(const test_config& config) const = 0;
};

class ping_test_validator final : public test_config_validator {
 public:
  [[nodiscard]] bool is_valid(const test_config& config) const override;
  [[nodiscard]] std::string get_validation_error(const test_config& config) const override;
};

class connect_test_validator final : public test_config_validator {
 public:
  [[nodiscard]] bool is_valid(const test_config& config) const override;
  [[nodiscard]] std::string get_validation_error(const test_config& config) const override;
};

class url_test_validator final : public test_config_validator {
 public:
  [[nodiscard]] bool is_valid(const test_config& config) const override;
  [[nodiscard]] std::string get_validation_error(const test_config& config) const override;
};

class test_config_validator_factory {
 public:
  static std::unique_ptr<test_config_validator> create(test_method method);
};