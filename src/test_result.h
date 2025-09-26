#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <sstream>

class test_result {
private:
    bool success_;
    long duration_ms_;
    std::chrono::system_clock::time_point timestamp_;
    std::optional<std::string> error_;

public:
    // Constructors
    test_result(bool success_val, long duration_ms_val,
                std::chrono::system_clock::time_point timestamp_val,
                const std::optional<std::string>& error_val = std::nullopt);

    // Convenience constructors
    static test_result create_success(long duration_ms_val);
    static test_result create_failure(const std::string& error_msg, long duration_ms_val = 0);

    // Getters
    [[nodiscard]] bool is_success() const { return success_; }
    [[nodiscard]] bool is_failure() const { return !success_; }
    [[nodiscard]] long get_duration_ms() const { return duration_ms_; }
    [[nodiscard]] const std::chrono::system_clock::time_point& get_timestamp() const { return timestamp_; }
    [[nodiscard]] const std::optional<std::string>& get_error() const { return error_; }
    [[nodiscard]] bool has_error() const { return error_.has_value(); }

    // Setters with validation
    void set_success(bool success_val);
    void set_duration_ms(long duration_ms_val);
    void set_timestamp(const std::chrono::system_clock::time_point& timestamp_val);
    void set_error(const std::string& error_val);
    void clear_error();

    // Validation methods
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] std::string get_validation_error() const;

    // Utility methods
    [[nodiscard]] std::string to_string() const;

private:
    void validate_parameters() const;
};