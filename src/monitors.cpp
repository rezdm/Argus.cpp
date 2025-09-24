#include "monitors.h"
#include "network_test_ping.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <numeric>
#include <algorithm>

monitors::monitors(const monitor_config& config) : running_(false) {
    // Set global ping implementation from config
    network_test_ping::set_ping_implementation(config.ping_impl);
    spdlog::info("Using ping implementation: {}", to_string(config.ping_impl));

    // Create thread pool - use reasonable size based on monitor count and hardware
    const size_t num_monitors = std::accumulate(config.monitors.begin(), config.monitors.end(), 0UL,
        [](size_t sum, const group& g) { return sum + g.destinations.size(); });

    // Calculate optimal thread pool size: balance between concurrency and resource usage
    const size_t hardware_threads = std::thread::hardware_concurrency();
    const size_t pool_size = std::min({
        std::max(4UL, hardware_threads),  // At least 4 threads
        num_monitors / 4 + 1,             // 1 thread per 4 monitors
        24UL                              // Cap at 24 threads
    });

    thread_pool_ = std::make_shared<thread_pool>(pool_size);
    scheduler_ = std::make_unique<async_scheduler>(thread_pool_);

    spdlog::info("Created thread pool with {} threads for {} monitors", thread_pool_->thread_count(), num_monitors);

    int total_monitors = 0;

    for (const auto& group : config.monitors) {
        std::string group_name = group.group_name;  // Store to avoid warning
        spdlog::info("Initializing monitor group: {}", group_name);

        for (const auto& dest : group.destinations) {
            try {
                const std::string key = group.group_name + ":" + dest.name;
                const auto state = std::make_shared<monitor_state>(dest, group);
                monitors_map_[key] = state;
                ++total_monitors;

                std::string test_desc = state->get_test_description();  // Store to avoid warning
                spdlog::debug("Initialized monitor: {} ({})", dest.name, test_desc);
            } catch (const std::exception& e) {
                spdlog::error("Failed to initialize monitor {}: {}", dest.name, e.what());
                throw std::runtime_error("Invalid test configuration for " + dest.name);
            }
        }
    }

    spdlog::info("Initialized {} monitors across {} groups", total_monitors, config.monitors.size());
}

monitors::~monitors() {
    stop_monitoring();
}

void monitors::start_monitoring() {
    if (running_.exchange(true)) {
        return; // Already running
    }

    spdlog::info("Starting async monitoring tasks");

    // Start the scheduler
    scheduler_->start();

    // Schedule all monitors
    for (const auto& [key, state] : monitors_map_) {
        schedule_monitor_test(state);
        spdlog::debug("Scheduled async monitor: {} (interval: {}s)", key, state->get_destination().interval);
    }

    spdlog::info("All {} monitoring tasks scheduled with {} threads", monitors_map_.size(), thread_pool_->thread_count());
}

void monitors::stop_monitoring() {
    if (!running_.exchange(false)) {
        return; // Already stopped
    }

    spdlog::info("Stopping async monitoring tasks");

    // Cancel all scheduled tasks
    for (const auto task_id : scheduled_task_ids_) {
        scheduler_->cancel_task(task_id);
    }
    scheduled_task_ids_.clear();

    // Stop the scheduler
    scheduler_->stop();

    spdlog::info("All monitoring tasks stopped");
}

void monitors::schedule_monitor_test(const std::shared_ptr<monitor_state>& state) {
    const auto interval = std::chrono::seconds(state->get_destination().interval);

    // Schedule the repeating task
    const auto task_id = scheduler_->schedule_repeating(interval, [this, state]() {
        if (running_) {
            perform_test_async(state);
        }
    });

    scheduled_task_ids_.push_back(task_id);
}

void monitors::perform_test_async(const std::shared_ptr<monitor_state>& state) {
    try {
        // Execute test asynchronously
        auto future = execute_test_async(state);

        // Handle result asynchronously to avoid blocking the scheduler
        thread_pool_->enqueue([this, state, future = std::move(future)]() mutable {
            try {
                const auto result = future.get();
                state->add_result(result);

                // Log significant status changes
                if (!result.success && state->get_current_status() != monitor_status::ok) {
                    spdlog::warn("Monitor {} status: {} (consecutive failures: {})",
                                 state->get_destination().name,
                                 to_string(state->get_current_status()),
                                 state->get_consecutive_failures());
                } else if (result.success && state->get_current_status() == monitor_status::ok &&
                           state->get_consecutive_successes() == state->get_destination().reset) {
                    spdlog::info("Monitor {} recovered to OK status", state->get_destination().name);
                }
            } catch (const std::exception& e) {
                spdlog::error("Error processing test result for {}: {}", state->get_destination().name, e.what());
                // Create failure result
                const test_result failure_result{false, 0, std::chrono::system_clock::now(), e.what()};
                state->add_result(failure_result);
            }
        });
    } catch (const std::exception& e) {
        spdlog::error("Error scheduling test for {}: {}", state->get_destination().name, e.what());
    }
}

std::future<test_result> monitors::execute_test_async(const std::shared_ptr<monitor_state>& state) {
    // Create a packaged task for the test execution
    const auto task = std::make_shared<std::packaged_task<test_result()>>([state]() {
        try {
            const auto& dest = state->get_destination();
            spdlog::trace("Executing {} test for {}", to_string(dest.test.test_method_type), dest.name);

            auto result = state->get_test_implementation()->execute(dest.test, dest.timeout);

            spdlog::trace("Test {} for {} completed in {}ms: {}",
                         to_string(dest.test.test_method_type),
                         dest.name,
                         result.duration_ms,
                         result.success ? "SUCCESS" : "FAILURE");

            return result;
        } catch (const std::exception& e) {
            spdlog::debug("Test failed for {}: {}", state->get_destination().name, e.what());
            return test_result{false, 0, std::chrono::system_clock::now(), e.what()};
        }
    });

    auto future = task->get_future();

    // Execute the task (this returns immediately, task runs in background)
    (*task)();

    return future;
}

const std::map<std::string, std::shared_ptr<monitor_state>>& monitors::get_monitors_map() const {
    return monitors_map_;
}

size_t monitors::active_tasks() const {
    return thread_pool_ ? thread_pool_->pending_tasks() : 0;
}

size_t monitors::scheduled_tasks() const {
    return scheduler_ ? scheduler_->scheduled_count() : 0;
}

