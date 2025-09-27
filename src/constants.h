#pragma once

namespace argus {
namespace constants {

// Default configuration values
constexpr int DEFAULT_CACHE_DURATION_SECONDS = 30;
constexpr int DEFAULT_THREAD_POOL_SIZE = 0;  // Auto-calculate
constexpr int MAX_HISTORY_RECORDS = 1000;

// Timeout limits and defaults (milliseconds)
constexpr int MAX_PING_TIMEOUT_MS = 300000;  // 5 minutes
constexpr int MILLISECONDS_PER_SECOND = 1000;
constexpr int MICROSECONDS_PER_MILLISECOND = 1000;

// Socket options
constexpr int SOCKET_OPTION_ENABLE = 1;

// Return codes
constexpr int SOCKET_ERROR = -1;
constexpr int SUCCESS_CODE = 0;

// Logging constants
constexpr int LOG_FLUSH_INTERVAL_MS = 100;
constexpr double PERCENTAGE_MULTIPLIER = 100.0;

// File paths
constexpr const char* DEFAULT_LOG_PATH = "/var/log/arguspp.log";

// Version information
constexpr const char* VERSION = "1.0.0";
constexpr const char* APPLICATION_NAME = "Argus++";

} // namespace constants
} // namespace argus