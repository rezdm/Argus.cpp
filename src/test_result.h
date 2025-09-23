#pragma once

#include <chrono>
#include <optional>
#include <string>

class test_result_impl {
public:
    test_result_impl(bool success, long duration_ms, std::chrono::system_clock::time_point timestamp, const std::optional<std::string>& error = std::nullopt);
    
    [[nodiscard]] bool get_success() const { return success_; }
    [[nodiscard]] long get_duration_ms() const { return duration_ms_; }
    [[nodiscard]] std::chrono::system_clock::time_point get_timestamp() const { return timestamp_; }
    [[nodiscard]] const std::optional<std::string>& get_error() const { return error_; }

private:
    bool success_;
    long duration_ms_;
    std::chrono::system_clock::time_point timestamp_;
    std::optional<std::string> error_;
};