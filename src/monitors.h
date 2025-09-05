#pragma once

#include "types.h"
#include "monitor_state.h"
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <future>

class monitors {
public:
    explicit monitors(const monitor_config& config);
    ~monitors();
    
    void start_monitoring();
    void stop_monitoring();
    
    [[nodiscard]] const std::map<std::string, std::shared_ptr<monitor_state>>& get_monitors_map() const;

private:
    std::map<std::string, std::shared_ptr<monitor_state>> monitors_map_;
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_;
    
    void perform_test(std::shared_ptr<monitor_state> state);

    static test_result execute_test(std::shared_ptr<monitor_state> state);
    void monitor_worker(std::shared_ptr<monitor_state> state);
};