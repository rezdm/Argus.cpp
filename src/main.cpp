#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "types.h"
#include "monitor_config.h"
#include "monitors.h"
#include "web_server.h"

class main_application {
private:
    std::shared_ptr<monitors> monitors_instance;
    std::shared_ptr<web_server> server_instance;
    static main_application* instance_;

public:
    explicit main_application(const std::string& config_path) {
        spdlog::info("Starting Argus++ Monitor with config: {}", config_path);
        log_memory_usage("Startup");

        auto config = monitor_config::load_config(config_path);
        std::string config_name = config.name;  // Store in variable to avoid warning
        spdlog::info("Loaded configuration for instance: {}", config_name);
        log_memory_usage("Config loaded");

        monitors_instance = std::make_shared<monitors>(config);
        log_memory_usage("Monitors initialized");

        server_instance = std::make_shared<web_server>(config, monitors_instance->get_monitors_map());
        monitors_instance->start_monitoring();
        
        spdlog::info("Argus++ Monitor initialization complete");
        log_memory_usage("Fully started");

        instance_ = this;
    }

    void shutdown() {
        spdlog::info("Shutting down Argus++ Monitor");

        if (server_instance) {
            server_instance->stop();
        }

        if (monitors_instance) {
            monitors_instance->stop_monitoring();
        }

        spdlog::info("Argus++ Monitor shutdown complete");
    }

    static void signal_handler(int signal) {
        spdlog::info("Received shutdown signal: {}", signal);
        if (instance_) {
            instance_->shutdown();
        }
        std::exit(0);
    }

private:
    static void log_memory_usage(const std::string& phase) {
        // Simple memory logging - can be enhanced with system-specific calls
        spdlog::info("Memory [{}]: Phase completed", phase);
    }
};

main_application* main_application::instance_ = nullptr;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        spdlog::error("Usage: {} <config.json>", argv[0]);
        return 1;
    }

    // Set up logging
    const auto logger = spdlog::stdout_color_mt("argus_plus");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    // Set process ID for logging
    auto pid = getpid();
    spdlog::info("Starting Argus++ Monitor version 1.0.0 (PID: {})", pid);

    try {
        // Set up signal handlers
        std::signal(SIGINT, main_application::signal_handler);
        std::signal(SIGTERM, main_application::signal_handler);

        auto app = std::make_unique<main_application>(argv[1]);

        spdlog::info("Argus++ Monitor started successfully. Press Ctrl+C to stop.");

        // Keep the main thread alive
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        spdlog::error("Error starting Argus++ Monitor: {}", e.what());
        return 1;
    }

    return 0;
}

