cmake_minimum_required(VERSION 3.20)
project(ArgusPlus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified, defaulting to Debug")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Enable parallel compilation by default
if(NOT CMAKE_BUILD_PARALLEL_LEVEL)
    include(ProcessorCount)
    ProcessorCount(N)
    if(NOT N EQUAL 0)
        set(CMAKE_BUILD_PARALLEL_LEVEL ${N} CACHE STRING "Default parallel build level")
        message(STATUS "Setting parallel build level to ${N} cores")
    endif()
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Find systemd (optional)
pkg_check_modules(SYSTEMD libsystemd)

# Library version configuration - can be overridden via command line
set(NLOHMANN_JSON_VERSION "v3.11.3" CACHE STRING "nlohmann/json version")
set(SPDLOG_VERSION "v1.12.0" CACHE STRING "spdlog version")
set(HTTPLIB_VERSION "v0.14.3" CACHE STRING "cpp-httplib version")

# Find nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG ${NLOHMANN_JSON_VERSION}
    )
    FetchContent_MakeAvailable(nlohmann_json)
    message(STATUS "Using nlohmann/json version: ${NLOHMANN_JSON_VERSION}")
endif()

# Find spdlog
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG ${SPDLOG_VERSION}
    )
    FetchContent_MakeAvailable(spdlog)
    message(STATUS "Using spdlog version: ${SPDLOG_VERSION}")
endif()

# Find httplib (cpp-httplib) - configure OpenSSL support before fetching
set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "Require OpenSSL for httplib")
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE ON CACHE BOOL "Use OpenSSL if available for httplib")

find_package(httplib QUIET)
if(NOT httplib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG ${HTTPLIB_VERSION}
    )
    FetchContent_MakeAvailable(httplib)
    message(STATUS "Using cpp-httplib version: ${HTTPLIB_VERSION}")
endif()

# Add executable
add_executable(arguspp
    main.cpp
    monitor_config.cpp
    monitors.cpp
    web_server.cpp
    test_result.cpp
    monitor_state.cpp
    test_factory.cpp
    network_test_ping.cpp
    network_test_connect.cpp
    network_test_url.cpp
    thread_pool.cpp
    async_scheduler.cpp
    network_test_async.cpp
)

# Link libraries
target_link_libraries(arguspp
    PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    httplib::httplib
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Link systemd if available
if(SYSTEMD_FOUND)
    target_link_libraries(arguspp PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(arguspp PRIVATE ${SYSTEMD_INCLUDE_DIRS})
    target_compile_definitions(arguspp PRIVATE HAVE_SYSTEMD)
    message(STATUS "Found systemd: ${SYSTEMD_VERSION}")
else()
    message(STATUS "systemd not found - systemd journal logging will be disabled")
endif()

# Configure httplib with OpenSSL
target_compile_definitions(arguspp PRIVATE
    HTTPLIB_REQUIRE_OPENSSL=ON
    HTTPLIB_USE_OPENSSL_IF_AVAILABLE=ON
)

# Compiler-specific options
if(MSVC)
    target_compile_options(arguspp PRIVATE /W4)
else()
    target_compile_options(arguspp PRIVATE 
        -Wall -Wextra -Wpedantic
        # Suppress the specific fmt dangling reference warnings that are false positives
        -Wno-dangling-reference
    )
endif()

# Set output directory
set_target_properties(arguspp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

