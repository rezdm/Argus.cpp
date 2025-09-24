#pragma once

#include "types.h"
#include "monitor_state.h"
#include "thread_pool.h"
#include "async_scheduler.h"
#include <map>
#include <memory>
#include <atomic>
#include <future>
#include <vector>

class monitors {
public:
    explicit monitors(const monitor_config& config);
    ~monitors();

    void start_monitoring();
    void stop_monitoring();

    [[nodiscard]] const std::map<std::string, std::shared_ptr<monitor_state>>& get_monitors_map() const;

    // Performance metrics
    [[nodiscard]] size_t active_tasks() const;
    [[nodiscard]] size_t scheduled_tasks() const;

    // Get the shared thread pool for other components
    [[nodiscard]] std::shared_ptr<thread_pool> get_thread_pool() const { return thread_pool_; }

private:
    std::map<std::string, std::shared_ptr<monitor_state>> monitors_map_;
    std::shared_ptr<thread_pool> thread_pool_;
    std::unique_ptr<async_scheduler> scheduler_;
    std::vector<size_t> scheduled_task_ids_;
    std::atomic<bool> running_;

    // Async test execution
    void perform_test_async(const std::shared_ptr<monitor_state>& state);
    static std::future<test_result> execute_test_async(const std::shared_ptr<monitor_state>& state);
    void schedule_monitor_test(const std::shared_ptr<monitor_state>& state);
};