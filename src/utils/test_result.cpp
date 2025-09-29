#include "test_result.h"

#include <stdexcept>

test_result::test_result(const bool success_val, const long duration_ms_val, const std::chrono::system_clock::time_point timestamp_val, const std::optional<std::string>& error_val)
    : success_(success_val), duration_ms_(duration_ms_val), timestamp_(timestamp_val), error_(error_val) {
  validate_parameters();
}

test_result test_result::create_success(const long duration_ms_val) { return test_result(true, duration_ms_val, std::chrono::system_clock::now()); }

test_result test_result::create_failure(const std::string& error_msg, const long duration_ms_val) {
  return test_result(false, duration_ms_val, std::chrono::system_clock::now(), error_msg);
}

void test_result::set_success(const bool success_val) { success_ = success_val; }

void test_result::set_duration_ms(const long duration_ms_val) {
  if (duration_ms_val < 0) {
    throw std::invalid_argument("Duration cannot be negative");
  }
  duration_ms_ = duration_ms_val;
}

void test_result::set_timestamp(const std::chrono::system_clock::time_point& timestamp_val) { timestamp_ = timestamp_val; }

void test_result::set_error(const std::string& error_val) { error_ = error_val; }

void test_result::clear_error() { error_.reset(); }

bool test_result::is_valid() const { return duration_ms_ >= 0; }

std::string test_result::get_validation_error() const {
  if (duration_ms_ < 0) return "Duration cannot be negative";
  return "";
}

std::string test_result::to_string() const {
  std::ostringstream oss;
  oss << "TestResult{success=" << success_ << ", duration=" << duration_ms_ << "ms";
  if (error_.has_value()) {
    oss << ", error='" << error_.value() << "'";
  }
  oss << "}";
  return oss.str();
}

void test_result::validate_parameters() const {
  if (!is_valid()) {
    throw std::invalid_argument("Invalid test result parameters: " + get_validation_error());
  }
}