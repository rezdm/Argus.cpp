#include "network_test_cmd.h"

#include <spdlog/spdlog.h>
#include <sys/wait.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "../monitoring/test_config.h"
#include "../utils/test_result.h"

test_result network_test_cmd::execute(const test_config& config, const int timeout_ms) const {
  const auto start_time = std::chrono::steady_clock::now();
  bool success = false;
  std::string error;

  try {
    validate_config(config);

    // Validate timeout
    if (timeout_ms <= 0 || timeout_ms > 300000) {  // Max 5 minutes
      throw std::invalid_argument("Invalid timeout: must be between 1ms and 300000ms");
    }

    const std::string& cmd = config.get_cmd_run().value();
    const int expected_exit_code = config.get_cmd_expect();
    std::string output;

    const int exit_code = execute_command(cmd, timeout_ms, output);

    // Check if exit code matches expected
    success = (exit_code == expected_exit_code);

    if (!success) {
      error = "Exit code " + std::to_string(exit_code) + " != expected " + std::to_string(expected_exit_code);
      if (!output.empty()) {
        // Include first line of output for debugging
        const size_t newline_pos = output.find('\n');
        const std::string first_line = (newline_pos != std::string::npos) ? output.substr(0, newline_pos) : output;
        if (!first_line.empty() && first_line.length() < 100) {
          error += ": " + first_line;
        }
      }
      spdlog::debug("Cmd test failed: {}", error);
    }
  } catch (const std::exception& e) {
    error = e.what();
    spdlog::debug("Cmd test failed for command '{}': {}", config.get_cmd_run().value_or("unknown"), error);
  }

  const auto end_time = std::chrono::steady_clock::now();
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  return {success, duration, std::chrono::system_clock::now(), error.empty() ? std::nullopt : std::optional(error)};
}

std::string network_test_cmd::get_description(const test_config& config) const {
  std::string desc = "Cmd: " + config.get_cmd_run().value_or("unknown");
  desc += " (expect=" + std::to_string(config.get_cmd_expect()) + ")";
  return desc;
}

void network_test_cmd::validate_config(const test_config& config) const {
  if (!config.get_cmd_run() || config.get_cmd_run()->empty()) {
    throw std::invalid_argument("Command is required for Cmd test");
  }
}

std::string network_test_cmd::shell_quote(const std::string& str) {
  // Simple shell quoting: wrap in single quotes and escape any single quotes in the string
  std::string quoted = "'";
  for (char c : str) {
    if (c == '\'') {
      quoted += "'\\''";  // End quote, add escaped quote, start quote again
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

int network_test_cmd::execute_command(const std::string& cmd, const int timeout_ms, std::string& output) {
  // Build command with timeout wrapper using shell built-in timeout if available
  // For portability, we'll use a simple approach with system() and parse exit code
  // Note: This is a simplified implementation. For production, consider using
  // fork/exec with proper timeout handling and signal management.

  std::array<char, 128> buffer{};
  output.clear();

  // Create command with timeout (GNU timeout command if available)
  // Fallback to regular execution if timeout command is not available
  const int timeout_sec = (timeout_ms + 999) / 1000;  // Round up to seconds
  std::string full_cmd;

  // Try to use timeout command for better timeout handling
  // Check if timeout command exists
  if (FILE* test_timeout = popen("command -v timeout 2>/dev/null", "r")) {
    char timeout_path[256];
    if (fgets(timeout_path, sizeof(timeout_path), test_timeout) != nullptr) {
      // timeout command exists - wrap the command in sh -c to ensure proper shell execution
      full_cmd = "timeout " + std::to_string(timeout_sec) + "s sh -c " + shell_quote(cmd) + " 2>&1";
    }
    pclose(test_timeout);
  }

  // If timeout command not found, use regular execution (popen uses sh -c by default)
  if (full_cmd.empty()) {
    full_cmd = cmd + " 2>&1";
  }

  spdlog::trace("Executing command: {}", full_cmd);

  // Execute command and capture output
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("Failed to execute command");
  }

  // Read output
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    output += buffer.data();
    // Limit output to prevent memory issues
    if (output.size() > 4096) {
      output = output.substr(0, 4096);
      break;
    }
  }

  // Get exit code
  const int exit_code_raw = pclose(pipe.release());

  // Extract actual exit code from wait status
  // WIFEXITED checks if child terminated normally
  // WEXITSTATUS extracts the exit status
  int exit_code;
  if (WIFEXITED(exit_code_raw)) {
    exit_code = WEXITSTATUS(exit_code_raw);
  } else if (WIFSIGNALED(exit_code_raw)) {
    // Process was terminated by signal (e.g., timeout)
    exit_code = 128 + WTERMSIG(exit_code_raw);
  } else {
    exit_code = exit_code_raw;
  }

  spdlog::trace("Command exit code: {}", exit_code);

  return exit_code;
}
