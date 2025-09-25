cmake_minimum_required(VERSION 3.20)
project(ArgusPlus VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified, defaulting to Debug")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Find systemd (optional)
pkg_check_modules(SYSTEMD libsystemd)

# Third-party library versions
set(NLOHMANN_JSON_VERSION "v3.11.3" CACHE STRING "nlohmann/json version")
set(SPDLOG_VERSION "v1.12.0" CACHE STRING "spdlog version")
set(HTTPLIB_VERSION "v0.14.3" CACHE STRING "cpp-httplib version")

# Configure httplib OpenSSL support
set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "Require OpenSSL for httplib")
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE ON CACHE BOOL "Use OpenSSL if available for httplib")

# Setup FetchContent for dependencies
include(FetchContent)

# Find or fetch nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG ${NLOHMANN_JSON_VERSION}
    )
    FetchContent_MakeAvailable(nlohmann_json)
    message(STATUS "Using fetched nlohmann/json version: ${NLOHMANN_JSON_VERSION}")
else()
    message(STATUS "Using system nlohmann/json")
endif()

# Find or fetch spdlog
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG ${SPDLOG_VERSION}
    )
    FetchContent_MakeAvailable(spdlog)
    message(STATUS "Using fetched spdlog version: ${SPDLOG_VERSION}")
else()
    message(STATUS "Using system spdlog")
endif()

# Find or fetch httplib (cpp-httplib)
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG ${HTTPLIB_VERSION}
    )
    FetchContent_MakeAvailable(httplib)
    message(STATUS "Using fetched cpp-httplib version: ${HTTPLIB_VERSION}")
else()
    message(STATUS "Using system cpp-httplib")
endif()

# Define source files
set(ARGUSPP_SOURCES
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
    http_client.cpp
    connection_tester.cpp
    ping_tester.cpp
    address_family_handler.cpp
)

# Create executable
add_executable(arguspp ${ARGUSPP_SOURCES})

# Link required libraries
target_link_libraries(arguspp
    PRIVATE
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        httplib::httplib
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Configure httplib compile definitions
target_compile_definitions(arguspp
    PRIVATE
        HTTPLIB_REQUIRE_OPENSSL=ON
        HTTPLIB_USE_OPENSSL_IF_AVAILABLE=ON
)

# Configure systemd support (optional)
if(SYSTEMD_FOUND)
    target_link_libraries(arguspp PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(arguspp PRIVATE ${SYSTEMD_INCLUDE_DIRS})
    target_compile_definitions(arguspp PRIVATE HAVE_SYSTEMD)
    message(STATUS "Found systemd: ${SYSTEMD_VERSION}")
else()
    message(STATUS "systemd not found - journal logging disabled")
endif()

# Configure compiler warnings and options
if(MSVC)
    target_compile_options(arguspp PRIVATE /W4)
else()
    target_compile_options(arguspp
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-dangling-reference  # Suppress false positives in fmt library
    )
endif()

# Set executable output directory
set_target_properties(arguspp
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

