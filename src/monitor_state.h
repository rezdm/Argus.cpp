#pragma once

#include "types.h"
#include "network_test.h"
#include <deque>
#include <memory>
#include <mutex>

class monitor_state {
public:
    monitor_state(const destination &dest, group grp);

    void add_result(const test_result& result);
    monitor_status get_current_status() const;
    const test_result* get_last_result() const;
    double get_uptime_percentage() const;
    std::vector<test_result> get_history() const;
    int get_consecutive_failures() const;
    int get_consecutive_successes() const;
    const std::string& get_group_name() const;
    std::string get_test_description() const;
    std::shared_ptr<network_test> get_test_implementation() const;
    
    const destination& get_destination() const { return destination_; }
    const group& get_group() const { return group_; }

private:
    const destination destination_;
    const group group_;
    
    mutable std::mutex mutex_;
    std::deque<test_result> history_;
    int consecutive_failures_ = 0;
    int consecutive_successes_ = 0;
    monitor_status current_status_ = monitor_status::ok;
    test_result last_result_;
    std::shared_ptr<network_test> test_implementation_;
    std::string test_description_;
    
    void update_status(bool test_success);
};