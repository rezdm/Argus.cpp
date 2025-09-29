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

# Enable testing
enable_testing()

# Option to run tests automatically during build
option(RUN_TESTS_ON_BUILD "Run tests automatically when building the main target" OFF)

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
    core/main.cpp
    core/logging.cpp
    monitoring/monitor_config.cpp
    monitoring/monitor_config_types.cpp
    monitoring/monitors.cpp
    monitoring/test_config.cpp
    monitoring/destination.cpp
    monitoring/group.cpp
    monitoring/monitor_state.cpp
    testers/test_factory.cpp
    testers/network_test_ping.cpp
    testers/network_test_connect.cpp
    testers/network_test_url.cpp
    testers/network_test_async.cpp
    testers/connection_tester.cpp
    testers/ping_tester.cpp
    network/address_family_handler.cpp
    network/http_client.cpp
    web/web_server.cpp
    utils/thread_pool.cpp
    utils/async_scheduler.cpp
    utils/test_result.cpp
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

# Create test executable
set(TEST_SOURCES
    network/address_family_handler.cpp
    core/logging.cpp
)

add_executable(test_ip_utils tests/test_ip_utils.cpp ${TEST_SOURCES})

# Link test executable with required libraries
target_link_libraries(test_ip_utils
    PRIVATE
        spdlog::spdlog
        Threads::Threads
)

# Add test to CTest
add_test(NAME ip_utils_test COMMAND test_ip_utils)

# Set test properties
set_tests_properties(ip_utils_test PROPERTIES
    TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Create custom target to build and run tests automatically
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_ip_utils
    COMMENT "Building and running all tests"
    VERBATIM
)

# Optionally make main target depend on tests
if(RUN_TESTS_ON_BUILD)
    add_custom_target(run_tests_auto
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS test_ip_utils
        COMMENT "Running tests automatically before building main target"
        VERBATIM
    )
    add_dependencies(arguspp run_tests_auto)
    message(STATUS "Tests will run automatically when building main target")
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
    )

    # Add GCC-specific warning suppression (not supported by Clang/FreeBSD)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(arguspp PRIVATE -Wno-dangling-reference)
    endif()
endif()

# Set executable output directory
set_target_properties(arguspp
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation configuration
set(CMAKE_INSTALL_PREFIX "/opt/arguspp" CACHE PATH "Installation directory")

# Install executable
install(TARGETS arguspp
    RUNTIME DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Configure and install default configuration
configure_file(../config/example_config.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.json
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config.json
    DESTINATION etc
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Install HTML templates
install(DIRECTORY ../templates/
    DESTINATION share/templates
    FILES_MATCHING PATTERN "*.html"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Install systemd service file if systemd is available
if(SYSTEMD_FOUND)
    configure_file(../arguspp.service.in
        ${CMAKE_CURRENT_BINARY_DIR}/arguspp.service
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/arguspp.service
        DESTINATION /etc/systemd/system
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )

    # Create a post-install script for systemd setup
    install(CODE "
        message(STATUS \"Installing systemd service...\")
        execute_process(COMMAND systemctl daemon-reload
            RESULT_VARIABLE DAEMON_RELOAD_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(DAEMON_RELOAD_RESULT EQUAL 0)
            message(STATUS \"Systemd service installed successfully\")
            message(STATUS \"To enable and start the service, run:\")
            message(STATUS \"  sudo systemctl enable arguspp\")
            message(STATUS \"  sudo systemctl start arguspp\")
        else()
            message(WARNING \"Failed to reload systemd daemon. You may need to run 'sudo systemctl daemon-reload' manually.\")
        endif()
    ")
endif()

# Create necessary directories and set capabilities
install(CODE "
    # Create argus user and group if they don't exist
    message(STATUS \"Creating argus user and group...\")
    execute_process(
        COMMAND id -u argus
        RESULT_VARIABLE USER_EXISTS
        OUTPUT_QUIET ERROR_QUIET
    )

    if(NOT USER_EXISTS EQUAL 0)
        execute_process(
            COMMAND useradd -r -s /bin/false -d /var/lib/arguspp argus
            RESULT_VARIABLE CREATE_USER_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )

        if(CREATE_USER_RESULT EQUAL 0)
            message(STATUS \"Created argus system user\")
        else()
            message(WARNING \"Failed to create argus user. You may need to create it manually:\")
            message(WARNING \"  sudo useradd -r -s /bin/false -d /var/lib/arguspp argus\")
        endif()
    else()
        message(STATUS \"User 'argus' already exists\")
    endif()

    # Create log directory
    file(MAKE_DIRECTORY \"/var/log/arguspp\")

    # Create runtime directory
    file(MAKE_DIRECTORY \"/var/lib/arguspp\")

    # Set ownership of all installed files and directories to argus user
    execute_process(
        COMMAND chown -R argus:argus \${CMAKE_INSTALL_PREFIX} /var/log/arguspp /var/lib/arguspp
        RESULT_VARIABLE CHOWN_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )

    if(CHOWN_RESULT EQUAL 0)
        message(STATUS \"Set ownership of installation and system directories to argus user\")
    else()
        message(WARNING \"Failed to set ownership. You may need to run:\")
        message(WARNING \"  sudo chown -R argus:argus \${CMAKE_INSTALL_PREFIX} /var/log/arguspp /var/lib/arguspp\")
    endif()

    # Ensure parent directories are accessible by argus user
    get_filename_component(PARENT_DIR \${CMAKE_INSTALL_PREFIX} DIRECTORY)
    execute_process(
        COMMAND chmod o+x \${PARENT_DIR}
        RESULT_VARIABLE CHMOD_PARENT_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )

    if(CHMOD_PARENT_RESULT EQUAL 0)
        message(STATUS \"Set execute permission on parent directory\")
    else()
        message(WARNING \"Failed to set parent directory permissions. You may need to run:\")
        message(WARNING \"  sudo chmod o+x \${PARENT_DIR}\")
    endif()

    # Set capabilities for ping functionality
    message(STATUS \"Setting capabilities for ping functionality...\")
    execute_process(
        COMMAND setcap cap_net_raw+ep \${CMAKE_INSTALL_PREFIX}/bin/arguspp
        RESULT_VARIABLE SETCAP_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )

    if(SETCAP_RESULT EQUAL 0)
        message(STATUS \"Capabilities set successfully\")
    else()
        message(WARNING \"Failed to set capabilities. To enable ping functionality, run:\")
        message(WARNING \"  sudo setcap cap_net_raw+ep \${CMAKE_INSTALL_PREFIX}/bin/arguspp\")
        message(WARNING \"Or enable unprivileged ICMP sockets:\")
        message(WARNING \"  sudo sysctl -w net.ipv4.ping_group_range='0 65535'\")
    endif()

    # Display installation summary
    message(STATUS \"\")
    message(STATUS \"=== Argus++ Installation Complete ===\")
    message(STATUS \"Installation directory: \${CMAKE_INSTALL_PREFIX}\")
    message(STATUS \"Executable: \${CMAKE_INSTALL_PREFIX}/bin/arguspp\")
    message(STATUS \"Config file: \${CMAKE_INSTALL_PREFIX}/etc/config.json\")
    message(STATUS \"Templates: \${CMAKE_INSTALL_PREFIX}/share/templates/\")
    message(STATUS \"\")
    message(STATUS \"To run: \${CMAKE_INSTALL_PREFIX}/bin/arguspp \${CMAKE_INSTALL_PREFIX}/etc/config.json\")
    if(SYSTEMD_FOUND)
        message(STATUS \"Or use systemd: sudo systemctl start arguspp\")
    endif()
    message(STATUS \"\")
")

# Create uninstall target
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    @ONLY
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    COMMENT "Uninstalling Argus++ and cleaning up system files..."
)

