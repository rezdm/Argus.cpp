#include "monitor_state.h"
#include "test_factory.h"
#include <algorithm>
#include <utility>
#include <spdlog/spdlog.h>

monitor_state::monitor_state(const destination& dest, group  grp) : destination_(dest), group_(std::move(grp)), last_result_(false, 0, std::chrono::system_clock::now()) {
    test_implementation_ = test_factory::get_test(dest.test.test_method_type);
    test_description_ = test_factory::validate_and_describe(dest.test);
}

void monitor_state::add_result(const test_result& result) {
    std::lock_guard lock(mutex_);
    
    last_result_ = result;
    
    // Add to history and maintain size limit
    history_.push_back(result);
    const int max_history = std::min(destination_.history, 1000); // Cap at 1000 records max
    while (static_cast<int>(history_.size()) > max_history) {
        history_.pop_front();
    }
    
    update_status(result.success);
}

void monitor_state::update_status(const bool test_success) {
    if (test_success) {
        consecutive_successes_++;
        consecutive_failures_ = 0;
        
        // Check if we should reset from warning/failure state
        if (current_status_ != monitor_status::ok && 
            consecutive_successes_ >= destination_.reset) {
            current_status_ = monitor_status::ok;
            consecutive_successes_ = 0;
        }
    } else {
        consecutive_failures_++;
        consecutive_successes_ = 0;
        
        // Update status based on failure thresholds
        if (consecutive_failures_ >= destination_.failure) {
            current_status_ = monitor_status::failure;
        } else if (consecutive_failures_ >= destination_.warning) {
            current_status_ = monitor_status::warning;
        }
    }
}

monitor_status monitor_state::get_current_status() const {
    std::lock_guard lock(mutex_);
    return current_status_;
}

const test_result* monitor_state::get_last_result() const {
    std::lock_guard lock(mutex_);
    return &last_result_;
}

double monitor_state::get_uptime_percentage() const {
    std::lock_guard lock(mutex_);
    
    if (history_.empty()) {
        return 0.0;
    }

    const int successful = std::ranges::count_if(history_, [](const test_result& r) { return r.success; });
    return static_cast<double>(successful) / history_.size() * 100.0;
}

std::vector<test_result> monitor_state::get_history() const {
    std::lock_guard lock(mutex_);
    return std::vector(history_.begin(), history_.end());
}

int monitor_state::get_consecutive_failures() const {
    std::lock_guard lock(mutex_);
    return consecutive_failures_;
}

int monitor_state::get_consecutive_successes() const {
    std::lock_guard lock(mutex_);
    return consecutive_successes_;
}

const std::string& monitor_state::get_group_name() const {
    return group_.group_name;
}

std::string monitor_state::get_test_description() const {
    return test_description_;
}

std::shared_ptr<network_test> monitor_state::get_test_implementation() const {
    return test_implementation_;
}

void monitor_state::reset_consecutive_counts() {
    std::lock_guard lock(mutex_);
    consecutive_failures_ = 0;
    consecutive_successes_ = 0;
    current_status_ = monitor_status::ok;
    spdlog::debug("Reset consecutive counts for monitor: {}", destination_.name);
}