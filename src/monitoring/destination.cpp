#include "destination.h"

#include <stdexcept>

destination::destination(int sort_val, const std::string& name_val, int timeout_val, int warning_val, int failure_val, int reset_val, int interval_val, int history_val,
                         test_config test_val)
    : sort_(sort_val),
      name_(name_val),
      timeout_(timeout_val),
      warning_(warning_val),
      failure_(failure_val),
      reset_(reset_val),
      interval_(interval_val),
      history_(history_val),
      test_(std::move(test_val)) {
  validate_parameters();
}

void destination::set_sort(const int sort_val) { sort_ = sort_val; }

void destination::set_name(const std::string& name_val) {
  if (name_val.empty()) {
    throw std::invalid_argument("Destination name cannot be empty");
  }
  name_ = name_val;
}

void destination::set_timeout(const int timeout_val) {
  if (timeout_val <= 0) {
    throw std::invalid_argument("Timeout must be positive");
  }
  timeout_ = timeout_val;
}

void destination::set_warning(const int warning_val) {
  if (warning_val <= 0) {
    throw std::invalid_argument("Warning threshold must be positive");
  }
  warning_ = warning_val;
}

void destination::set_failure(const int failure_val) {
  if (failure_val <= 0) {
    throw std::invalid_argument("Failure threshold must be positive");
  }
  failure_ = failure_val;
}

void destination::set_reset(const int reset_val) {
  if (reset_val <= 0) {
    throw std::invalid_argument("Reset threshold must be positive");
  }
  reset_ = reset_val;
}

void destination::set_interval(const int interval_val) {
  if (interval_val <= 0) {
    throw std::invalid_argument("Interval must be positive");
  }
  interval_ = interval_val;
}

void destination::set_history(const int history_val) {
  if (history_val <= 0) {
    throw std::invalid_argument("History size must be positive");
  }
  history_ = history_val;
}

void destination::set_test(const test_config& test_val) {
  if (!test_val.is_valid()) {
    throw std::invalid_argument("Test configuration is invalid: " + test_val.get_validation_error());
  }
  test_ = test_val;
}

bool destination::is_valid() const { return !name_.empty() && timeout_ > 0 && warning_ > 0 && failure_ > 0 && reset_ > 0 && interval_ > 0 && history_ > 0 && test_.is_valid(); }

std::string destination::get_validation_error() const {
  if (name_.empty()) return "Destination name cannot be empty";
  if (timeout_ <= 0) return "Timeout must be positive";
  if (warning_ <= 0) return "Warning threshold must be positive";
  if (failure_ <= 0) return "Failure threshold must be positive";
  if (reset_ <= 0) return "Reset threshold must be positive";
  if (interval_ <= 0) return "Interval must be positive";
  if (history_ <= 0) return "History size must be positive";
  if (!test_.is_valid()) return "Test configuration is invalid: " + test_.get_validation_error();
  return "";
}

void destination::validate_parameters() const {
  if (!is_valid()) {
    throw std::invalid_argument("Invalid destination parameters: " + get_validation_error());
  }
}