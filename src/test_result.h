#pragma once

#include "types.h"
#include <chrono>
#include <optional>
#include <string>

class test_result_impl {
public:
    test_result_impl(bool success, long duration_ms, 
                    std::chrono::system_clock::time_point timestamp,
                    std::optional<std::string> error = std::nullopt);
    
    bool get_success() const { return success_; }
    long get_duration_ms() const { return duration_ms_; }
    std::chrono::system_clock::time_point get_timestamp() const { return timestamp_; }
    const std::optional<std::string>& get_error() const { return error_; }

private:
    bool success_;
    long duration_ms_;
    std::chrono::system_clock::time_point timestamp_;
    std::optional<std::string> error_;
};