#pragma once

#include "test_config.h"
#include <string>

class destination {
private:
    int sort_{};
    std::string name_;
    int timeout_{};
    int warning_{};
    int failure_{};
    int reset_{};
    int interval_{};
    int history_{};
    test_config test_;

public:

    // Constructors
    destination() = default;

    destination(int sort_val, const std::string& name_val, int timeout_val, int warning_val, int failure_val, int reset_val, int interval_val, int history_val, test_config test_val);

    // Getters
    [[nodiscard]] int get_sort() const { return sort_; }
    [[nodiscard]] const std::string& get_name() const { return name_; }
    [[nodiscard]] int get_timeout() const { return timeout_; }
    [[nodiscard]] int get_warning() const { return warning_; }
    [[nodiscard]] int get_failure() const { return failure_; }
    [[nodiscard]] int get_reset() const { return reset_; }
    [[nodiscard]] int get_interval() const { return interval_; }
    [[nodiscard]] int get_history() const { return history_; }
    [[nodiscard]] const test_config& get_test() const { return test_; }

    // Setters with validation
    void set_sort(int sort_val);
    void set_name(const std::string& name_val);
    void set_timeout(int timeout_val);
    void set_warning(int warning_val);
    void set_failure(int failure_val);
    void set_reset(int reset_val);
    void set_interval(int interval_val);
    void set_history(int history_val);
    void set_test(const test_config& test_val);

    // Validation methods
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] std::string get_validation_error() const;

private:
    void validate_parameters() const;
};