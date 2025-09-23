#include "monitors.h"
#include "network_test_ping.h"
#include <spdlog/spdlog.h>
#include <chrono>

monitors::monitors(const monitor_config& config) : running_(false) {
    // Set global ping implementation from config
    network_test_ping::set_ping_implementation(config.ping_impl);
    spdlog::info("Using ping implementation: {}", to_string(config.ping_impl));

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
    
    spdlog::info("Starting monitoring tasks");
    
    for (const auto& [key, state] : monitors_map_) {
        spdlog::debug("Scheduling monitor: {} (interval: {}s)", key, state->get_destination().interval);
        
        // Create a worker thread for each monitor
        worker_threads_.emplace_back([this, state]() {
            monitor_worker(state);
        });
    }
    
    spdlog::info("All monitoring tasks scheduled");
}

void monitors::stop_monitoring() {
    if (!running_.exchange(false)) {
        return; // Already stopped
    }
    
    spdlog::info("Stopping monitoring tasks");
    
    // Wait for all worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    spdlog::info("All monitoring tasks stopped");
}

void monitors::monitor_worker(const std::shared_ptr<monitor_state> &state) const {
    while (running_) {
        perform_test(state);
        
        // Sleep for the interval, but check running_ periodically
        const int interval_seconds = state->get_destination().interval;
        for (int i = 0; i < interval_seconds && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void monitors::perform_test(const std::shared_ptr<monitor_state> &state) {
    try {
        const auto result = execute_test(state);
        state->add_result(result);
        
        // Log significant status changes
        if (!result.success && state->get_current_status() != monitor_status::ok) {
            spdlog::warn("Monitor {} status: {} (consecutive failures: {})"
                , state->get_destination().name
                , to_string(state->get_current_status())
                , state->get_consecutive_failures()
            );
        } else if (result.success && state->get_current_status() == monitor_status::ok && state->get_consecutive_successes() == state->get_destination().reset) {
            spdlog::info("Monitor {} recovered to OK status", state->get_destination().name);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error executing test for {}: {}", state->get_destination().name, e.what());
    }
}

test_result monitors::execute_test(const std::shared_ptr<monitor_state> &state) {
    try {
        const auto& dest = state->get_destination();
        spdlog::trace("Executing {} test for {}", to_string(dest.test.test_method_type), dest.name);
        auto result = state->get_test_implementation()->execute(dest.test, dest.timeout);
        spdlog::trace("Test {} for {} completed in {}ms: {}"
            , to_string(dest.test.test_method_type)
            , dest.name
            , result.duration_ms, result.success ? "SUCCESS" : "FAILURE"
        );
        return result;
    } catch (const std::exception& e) {
        spdlog::debug("Test failed for {}: {}", state->get_destination().name, e.what());
        return {false, 0, std::chrono::system_clock::now(), e.what()};
    }
}

const std::map<std::string, std::shared_ptr<monitor_state>>& monitors::get_monitors_map() const {
    return monitors_map_;
}

