#include <fcntl.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#ifdef HAVE_SYSTEMD
#include <spdlog/sinks/systemd_sink.h>
#include <systemd/sd-daemon.h>
#endif

#include "../monitoring/monitor_config.h"
#include "../monitoring/monitors.h"
#include "../web/web_server.h"
#include "constants.h"
#include "logging.h"
#include "types.h"

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
  std::string config_path_;
  monitor_config current_config_;
  mutable std::mutex reload_mutex_;

 public:
  explicit main_application(const std::string& config_path, const bool daemon_mode = false, const bool systemd_mode = false)
      : daemon_mode_(daemon_mode), systemd_mode_(systemd_mode), config_path_(config_path) {
    LOG_STARTUP(config_path);
    log_memory_usage("Startup");

    try {
      current_config_ = monitor_config::load_config(config_path);
      const std::string config_name = current_config_.get_name();  // Store in variable to avoid warning
      argus::logging::logger::log_config_loaded(config_name);
      log_memory_usage("Config loaded");

      // Initialize monitors with graceful degradation
      try {
        monitors_instance = std::make_shared<monitors>(current_config_);
        log_memory_usage("Monitors initialized");
      } catch (const std::exception& e) {
        LOG_COMPONENT_FAILURE("monitors", e.what());
        // Create empty monitors instance for web server compatibility
        monitors_instance = nullptr;
      }

      // Initialize web server with graceful degradation
      try {
        if (monitors_instance) {
          server_instance = std::make_shared<web_server>(current_config_, monitors_instance->get_monitors_map(), monitors_instance->get_thread_pool());
        } else {
          // Create web server with empty monitor map and null thread pool
          std::map<std::string, std::shared_ptr<monitor_state>> empty_map;
          server_instance = std::make_shared<web_server>(current_config_, empty_map, nullptr);
        }
      } catch (const std::exception& e) {
        LOG_COMPONENT_FAILURE("web server", e.what());
        server_instance = nullptr;
      }

      // Start monitoring if available
      if (monitors_instance) {
        try {
          monitors_instance->start_monitoring();
          spdlog::info("Monitoring services started successfully");
        } catch (const std::exception& e) {
          spdlog::error("Failed to start monitoring: {}. Monitors created but not active.", e.what());
        }
      }

      spdlog::info("Argus Monitor initialization complete (graceful degradation applied where needed)");
      log_memory_usage("Fully started");

      // Notify systemd that we're ready
      if (systemd_mode_) {
        notify_systemd_ready();
      }

    } catch (const std::exception& e) {
      spdlog::critical("Critical failure during initialization: {}", e.what());
      throw;  // Re-throw critical errors
    }

    instance_ = this;
  }

  void shutdown() const {
    LOG_SHUTDOWN();

    if (server_instance) {
      server_instance->stop();
    }

    if (monitors_instance) {
      monitors_instance->stop_monitoring();
    }

    spdlog::info("Argus Monitor shutdown complete");
  }

  static void signal_handler(int signal) {
    if (signal == SIGHUP) {
      spdlog::info("Received SIGHUP signal: reloading configuration");
      if (instance_) {
        instance_->reload_config();
      }
    } else {
      spdlog::info("Received shutdown signal: {}", signal);
      if (instance_) {
        instance_->shutdown();
      }
      std::exit(0);
    }
  }

  void reload_config() {
    std::lock_guard<std::mutex> lock(reload_mutex_);

    argus::logging::logger::log_config_reload_start(config_path_);

    try {
      // Load new configuration and validate it
      monitor_config new_config = monitor_config::load_config(config_path_);
      argus::logging::logger::log_config_loaded(new_config.get_name());

      // Backup current instances before stopping (for rollback on failure)
      const auto backup_monitors = monitors_instance;
      const auto backup_server = server_instance;

      // Stop current monitoring gracefully
      if (monitors_instance) {
        spdlog::info("Stopping current monitors for reload");
        monitors_instance->stop_monitoring();
      }

      if (server_instance) {
        spdlog::info("Stopping web server for reload");
        server_instance->stop();
      }

      bool reload_successful = true;

      // Recreate monitors with new configuration
      std::shared_ptr<monitors> new_monitors_instance = nullptr;
      try {
        new_monitors_instance = std::make_shared<monitors>(new_config);
        spdlog::info("Recreated monitors with new configuration");
      } catch (const std::exception& e) {
        spdlog::error("Failed to recreate monitors with new config: {}.", e.what());
        reload_successful = false;
      }

      // Recreate web server with new configuration
      std::shared_ptr<web_server> new_server_instance = nullptr;
      if (reload_successful) {
        try {
          if (new_monitors_instance) {
            new_server_instance = std::make_shared<web_server>(new_config, new_monitors_instance->get_monitors_map(), new_monitors_instance->get_thread_pool());
          } else {
            std::map<std::string, std::shared_ptr<monitor_state>> empty_map;
            new_server_instance = std::make_shared<web_server>(new_config, empty_map, nullptr);
          }
          spdlog::info("Recreated web server with new configuration");
        } catch (const std::exception& e) {
          spdlog::error("Failed to recreate web server with new config: {}.", e.what());
          reload_successful = false;
        }
      }

      if (reload_successful) {
        // Update stored configuration and instances
        current_config_ = std::move(new_config);
        monitors_instance = new_monitors_instance;
        server_instance = new_server_instance;

        // Start monitoring with new configuration
        if (monitors_instance) {
          try {
            monitors_instance->start_monitoring();
            spdlog::info("Restarted monitoring with new configuration");
          } catch (const std::exception& e) {
            spdlog::error("Failed to start monitoring with new config: {}", e.what());
          }
        }

        // Also reload HTML template if it exists and the path is the same
        if (server_instance && current_config_.get_html_template() == new_config.get_html_template() && current_config_.get_html_template().has_value()) {
          spdlog::info("Reloading HTML template from existing path");
          server_instance->reload_html_template();
        }

        argus::logging::logger::log_config_reload_success();
      } else {
        // Rollback to previous configuration
        spdlog::warn("Configuration reload failed, rolling back to previous configuration");

        monitors_instance = backup_monitors;
        server_instance = backup_server;

        // Restart previous instances
        if (monitors_instance) {
          try {
            monitors_instance->start_monitoring();
            spdlog::info("Rolled back to previous monitoring configuration");
          } catch (const std::exception& e) {
            spdlog::error("Failed to restart previous monitoring config: {}", e.what());
          }
        }

        spdlog::error("Configuration reload failed, continuing with previous configuration");
      }

    } catch (const std::exception& e) {
      spdlog::error("Configuration reload failed: {}. Continuing with current configuration.", e.what());
    }
  }

 private:
  static void log_memory_usage(const std::string& phase) {
    // Simple memory logging - can be enhanced with system-specific calls
    spdlog::info("Memory [{}]: Phase completed", phase);
  }
};

main_application* main_application::instance_ = nullptr;

// Check if running under systemd
bool is_systemd_service() { return std::getenv("NOTIFY_SOCKET") != nullptr; }

// Notify systemd of readiness
void notify_systemd_ready() {
#ifdef HAVE_SYSTEMD
  const bool success = sd_notify(0, "READY=1") >= 0;
  argus::logging::logger::log_systemd_operation("service readiness", success);
#else
  if (std::getenv("NOTIFY_SOCKET")) {
    const std::string cmd = "systemd-notify --ready";
    const bool success = system(cmd.c_str()) == 0;
    argus::logging::logger::log_systemd_operation("service readiness", success);
  }
#endif
}

// Systemd watchdog ping
void notify_systemd_watchdog() {
#ifdef HAVE_SYSTEMD
  if (std::getenv("WATCHDOG_USEC")) {
    sd_notify(0, "WATCHDOG=1");
  }
#else
  if (std::getenv("WATCHDOG_USEC")) {
    const std::string cmd = "systemd-notify WATCHDOG=1";
    system(cmd.c_str());
  }
#endif
}

// Daemon implementation
bool daemonize() {
  // First fork
  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid > 0) {
    // Parent process exits
    exit(0);
  }

  // Become session leader
  if (setsid() < 0) {
    return false;
  }

  // Ignore SIGHUP
  signal(SIGHUP, SIG_IGN);

  // Second fork
  pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid > 0) {
    // First child exits
    exit(0);
  }

  // Change working directory to root (traditional daemon behavior)
  // Note: We'll change back to original directory after logging setup
  if (chdir("/") < 0) {
    return false;
  }

  // Set file creation mask
  umask(0);

  // Redirect stdin and stdout to /dev/null
  const int fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    return false;
  }

  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  // Keep stderr open initially for error reporting

  if (fd > STDERR_FILENO) {
    close(fd);
  }

  return true;
}

// Redirect stderr to /dev/null after successful startup
void redirect_stderr_to_null() {
  if (const int fd = open("/dev/null", O_WRONLY); fd >= 0) {
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) {
      close(fd);
    }
  }
}

void setup_logging(const bool daemon_mode, const bool systemd_mode, const std::string& log_file_path = "") {
  if (systemd_mode && log_file_path.empty()) {
    // For systemd mode without explicit log file, use systemd journal if available
#ifdef HAVE_SYSTEMD
    const auto systemd_logger = spdlog::systemd_logger_mt("argus");
    spdlog::set_default_logger(systemd_logger);
    spdlog::info("Logging to systemd journal");
#else
    spdlog::warn("systemd not available at compile time, using file logging");
    std::string log_path = argus::constants::DEFAULT_LOG_PATH;
    auto file_logger = spdlog::basic_logger_mt("argus", log_path);
    spdlog::set_default_logger(file_logger);
#endif
  } else if (daemon_mode || !log_file_path.empty()) {
    // For daemon mode or when log file is specified, log to file
    std::string log_path = log_file_path.empty() ? argus::constants::DEFAULT_LOG_PATH : log_file_path;
    const auto file_logger = spdlog::basic_logger_mt("argus", log_path);

    // Enable immediate flush for daemon mode to see logs in real-time
    if (daemon_mode) {
      file_logger->flush_on(spdlog::level::info);                       // Flush on every log message
      file_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");  // Ensure timestamps
    }

    spdlog::set_default_logger(file_logger);
    if (!daemon_mode) {
      spdlog::info("Logging to file: {}", log_path);
    }
  } else {
    // For normal mode, log to stdout
    const auto logger = spdlog::stdout_color_mt("argus");
    spdlog::set_default_logger(logger);
  }
  spdlog::set_level(spdlog::level::info);

  // For daemon mode, ensure immediate flushing for real-time log viewing
  if (daemon_mode) {
    spdlog::flush_every(std::chrono::milliseconds(argus::constants::LOG_FLUSH_INTERVAL_MS));
  }
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
    if (std::string flag = argv[arg_idx]; flag == "-d" || flag == "--daemon") {
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
  std::string config_path = argv[arg_idx];

  // Load config to get log file setting
  std::string log_file_path;
  if (!cmdline_log_file.empty()) {
    log_file_path = cmdline_log_file;
  } else {
    try {
      if (const auto config = monitor_config::load_config(config_path); config.get_log_file().has_value()) {
        log_file_path = config.get_log_file().value();
      }
    } catch (const std::exception& e) {
      std::cerr << "Error loading config for log file setting: " << e.what() << "\n";
      return 1;
    }
  }

  // Daemonize if requested (but not in systemd mode)
  if (daemon_mode && !systemd_mode) {
    // Convert relative paths to absolute paths before daemonizing
    std::string absolute_log_path = log_file_path;
    std::string absolute_config_path = config_path;

    if (char* cwd = getcwd(nullptr, 0)) {
      // Convert log file path if relative
      if (!log_file_path.empty() && log_file_path[0] != '/') {
        absolute_log_path = std::string(cwd) + "/" + log_file_path;
      }
      // Convert config file path if relative
      if (config_path[0] != '/') {
        absolute_config_path = std::string(cwd) + "/" + config_path;
      }
      free(cwd);
    }

    if (!daemonize()) {
      std::cerr << "Failed to daemonize" << std::endl;
      return 1;
    }

    // Update config_path for the daemon process
    config_path = absolute_config_path;

    // Setup logging only in the final daemon process with absolute path
    setup_logging(true, false, absolute_log_path);
  } else {
    // Set up logging normally for non-daemon mode
    setup_logging(false, systemd_mode, log_file_path);
    if (systemd_mode) {
      spdlog::info("Running in systemd mode");
    }
  }

  // Set process ID for logging
  auto pid = getpid();

  spdlog::info("Starting {} Monitor version {} (PID: {})", argus::constants::APPLICATION_NAME, argus::constants::VERSION, pid);

  try {
    spdlog::info("Setting up signal handlers...");
    // Set up signal handlers
    std::signal(SIGINT, main_application::signal_handler);
    std::signal(SIGTERM, main_application::signal_handler);
    std::signal(SIGHUP, main_application::signal_handler);

    spdlog::info("Creating main application...");
    auto app = std::make_unique<main_application>(config_path, daemon_mode, systemd_mode);

    spdlog::info("Argus Monitor started successfully. Press Ctrl+C to stop.");

    // Now that we've successfully started, redirect stderr for daemon mode
    if (daemon_mode && !systemd_mode) {
      redirect_stderr_to_null();
    }

    // Keep the main thread alive
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception& e) {
    spdlog::error("Error starting Argus Monitor: {}", e.what());
    return 1;
  }

  return 0;
}
