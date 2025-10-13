#pragma once

#include "network_test.h"

class network_test_cmd final : public network_test {
 public:
  [[nodiscard]] test_result execute(const test_config& config, int timeout_ms) const override;
  [[nodiscard]] std::string get_description(const test_config& config) const override;
  void validate_config(const test_config& config) const override;

 private:
  static std::string shell_quote(const std::string& str);
  static int execute_command(const std::string& cmd, int timeout_ms, std::string& output);
};
