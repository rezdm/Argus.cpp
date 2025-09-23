#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#ifdef HAVE_SYSTEMD
#include <spdlog/sinks/systemd_sink.h>
#endif

#include "types.h"
#include "monitor_config.h"
#include "monitors.h"
#include "web_server.h"

// Forward declarations for systemd functions
bool is_systemd_service();
void notify_systemd_ready();
void notify_systemd_watchdog();

class main_application {
private:
    std::shared_ptr<monitors> monitors_instance;
    std::shared_ptr<web_server> server_instance;
    static main_application* instance_;
    bool daemon_mode_;
    bool systemd_mode_;

public:
    explicit main_application(const std::string& config_path, bool daemon_mode = false, bool systemd_mode = false)
        : daemon_mode_(daemon_mode), systemd_mode_(systemd_mode) {
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

        // Notify systemd that we're ready
        if (systemd_mode_) {
            notify_systemd_ready();
        }

        instance_ = this;
    }

    void shutdown() const {
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

// Check if running under systemd
bool is_systemd_service() {
    return std::getenv("NOTIFY_SOCKET") != nullptr;
}

// Notify systemd of readiness
void notify_systemd_ready() {
    if (std::getenv("NOTIFY_SOCKET")) {
        // Simple implementation - in production you might want libsystemd
        std::string cmd = "systemd-notify --ready";
        if (system(cmd.c_str()) != 0) {
            spdlog::warn("Failed to notify systemd of readiness");
        } else {
            spdlog::info("Notified systemd of service readiness");
        }
    }
}

// Systemd watchdog ping
void notify_systemd_watchdog() {
    if (std::getenv("WATCHDOG_USEC")) {
        std::string cmd = "systemd-notify WATCHDOG=1";
        system(cmd.c_str());
    }
}

// Daemon implementation
bool daemonize() {
    // First fork
    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("First fork failed: {}", strerror(errno));
        return false;
    }
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }

    // Become session leader
    if (setsid() < 0) {
        spdlog::error("setsid failed: {}", strerror(errno));
        return false;
    }

    // Ignore SIGHUP
    signal(SIGHUP, SIG_IGN);

    // Second fork
    pid = fork();
    if (pid < 0) {
        spdlog::error("Second fork failed: {}", strerror(errno));
        return false;
    }
    if (pid > 0) {
        // First child exits
        exit(0);
    }

    // Change working directory to root
    if (chdir("/") < 0) {
        spdlog::error("chdir failed: {}", strerror(errno));
        return false;
    }

    // Set file creation mask
    umask(0);

    // Close all file descriptors
    for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
        close(fd);
    }

    // Redirect stdin, stdout, stderr to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        return false;
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) {
        close(fd);
    }

    return true;
}

void setup_logging(bool daemon_mode, bool systemd_mode, const std::string& log_file_path = "") {
    if (systemd_mode && log_file_path.empty()) {
        // For systemd mode without explicit log file, use systemd journal if available
#ifdef HAVE_SYSTEMD
        auto systemd_logger = spdlog::systemd_logger_mt("arguspp");
        spdlog::set_default_logger(systemd_logger);
        spdlog::info("Logging to systemd journal");
#else
        spdlog::warn("systemd not available at compile time, using file logging");
        std::string log_path = "/var/log/arguspp.log";
        auto file_logger = spdlog::basic_logger_mt("arguspp", log_path);
        spdlog::set_default_logger(file_logger);
#endif
    } else if (daemon_mode || !log_file_path.empty()) {
        // For daemon mode or when log file is specified, log to file
        std::string log_path = log_file_path.empty() ? "/var/log/arguspp.log" : log_file_path;
        auto file_logger = spdlog::basic_logger_mt("arguspp", log_path);
        spdlog::set_default_logger(file_logger);
        if (!daemon_mode) {
            spdlog::info("Logging to file: {}", log_path);
        }
    } else {
        // For normal mode, log to stdout
        auto logger = spdlog::stdout_color_mt("arguspp");
        spdlog::set_default_logger(logger);
    }
    spdlog::set_level(spdlog::level::info);
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [-d|--daemon] [-s|--systemd] [-l|--log-file <path>] <config.json>\n";
    std::cout << "Options:\n";
    std::cout << "  -d, --daemon              Run as daemon (detach from terminal)\n";
    std::cout << "  -s, --systemd             Run in systemd mode (no fork, journal logging)\n";
    std::cout << "  -l, --log-file <path>     Log to specified file (overrides config/systemd settings)\n";
    std::cout << "  config.json               Configuration file path\n";
    std::cout << "\nNote: systemd mode is automatically detected when NOTIFY_SOCKET is set\n";
}

int main(const int argc, char* argv[]) {
    bool daemon_mode = false;
    bool systemd_mode = false;
    std::string config_path;
    std::string cmdline_log_file;

    // Auto-detect systemd environment
    if (is_systemd_service()) {
        systemd_mode = true;
    }

    // Parse command line arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    int arg_idx = 1;
    while (arg_idx < argc - 1) {
        std::string flag = argv[arg_idx];
        if (flag == "-d" || flag == "--daemon") {
            daemon_mode = true;
            arg_idx++;
        } else if (flag == "-s" || flag == "--systemd") {
            systemd_mode = true;
            daemon_mode = false;  // systemd mode overrides daemon mode
            arg_idx++;
        } else if (flag == "-l" || flag == "--log-file") {
            if (arg_idx + 1 >= argc - 1) {
                std::cerr << "Error: --log-file requires a file path\n";
                print_usage(argv[0]);
                return 1;
            }
            cmdline_log_file = argv[arg_idx + 1];
            arg_idx += 2;
        } else {
            std::cerr << "Error: Unknown option " << flag << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (arg_idx != argc - 1) {
        std::cerr << "Error: Config file path is required\n";
        print_usage(argv[0]);
        return 1;
    }
    config_path = argv[arg_idx];

    // Load config to get log file setting
    std::string log_file_path;
    if (!cmdline_log_file.empty()) {
        log_file_path = cmdline_log_file;
    } else {
        try {
            auto config = monitor_config::load_config(config_path);
            if (config.log_file.has_value()) {
                log_file_path = config.log_file.value();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error loading config for log file setting: " << e.what() << "\n";
            return 1;
        }
    }

    // Set up logging before daemonizing
    setup_logging(false, systemd_mode, log_file_path);

    // Daemonize if requested (but not in systemd mode)
    if (daemon_mode && !systemd_mode) {
        spdlog::info("Daemonizing Argus++ Monitor...");
        if (!daemonize()) {
            spdlog::error("Failed to daemonize");
            return 1;
        }
        // Re-setup logging for daemon mode
        setup_logging(true, false, log_file_path);
    } else if (systemd_mode) {
        spdlog::info("Running in systemd mode");
    }

    // Set process ID for logging
    auto pid = getpid();
    spdlog::info("Starting Argus++ Monitor version 1.0.0 (PID: {})", pid);

    try {
        // Set up signal handlers
        std::signal(SIGINT, main_application::signal_handler);
        std::signal(SIGTERM, main_application::signal_handler);

        auto app = std::make_unique<main_application>(config_path, daemon_mode, systemd_mode);

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

