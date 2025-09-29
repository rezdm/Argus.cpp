#pragma once

// Core enums used across the system
enum class test_method { ping, connect, url };

enum class protocol { tcp, udp };

enum class monitor_status { pending, ok, warning, failure };

// Forward declarations for main classes
class test_config;
class destination;
class group;
class monitor_config;
class test_result;