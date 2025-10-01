cmake_minimum_required(VERSION 3.20)
project(Argus VERSION 1.0.0 LANGUAGES CXX)

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
set(ARGUS_SOURCES
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
    utils/task_recovery_policy.cpp
    utils/test_result.cpp
)

# Create executable
add_executable(argus ${ARGUS_SOURCES})

# Link required libraries
target_link_libraries(argus
    PRIVATE
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        httplib::httplib
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Configure httplib compile definitions
target_compile_definitions(argus
    PRIVATE
        HTTPLIB_REQUIRE_OPENSSL=ON
        HTTPLIB_USE_OPENSSL_IF_AVAILABLE=ON
)

# Configure systemd support (optional)
if(SYSTEMD_FOUND)
    target_link_libraries(argus PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(argus PRIVATE ${SYSTEMD_INCLUDE_DIRS})
    target_compile_definitions(argus PRIVATE HAVE_SYSTEMD)
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
    add_dependencies(argus run_tests_auto)
    message(STATUS "Tests will run automatically when building main target")
endif()

# Configure compiler warnings and options
if(MSVC)
    target_compile_options(argus PRIVATE /W4)
else()
    target_compile_options(argus
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    # Add GCC-specific warning suppression (not supported by Clang/FreeBSD)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(argus PRIVATE -Wno-dangling-reference)
    endif()
endif()

# Set executable output directory
set_target_properties(argus
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation configuration
set(CMAKE_INSTALL_PREFIX "/opt/argus" CACHE PATH "Installation directory")

# Install executable
install(TARGETS argus
    RUNTIME DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Configure and install default configuration
configure_file(../config/example_config.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/argus.json
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/argus.json
    DESTINATION etc
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Install HTML templates
install(DIRECTORY ../templates/
    DESTINATION share/templates
    FILES_MATCHING PATTERN "*.html"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Platform-specific service management detection and installation
set(SERVICE_TYPE "none")

# Detect init system based on platform and available tools
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(SYSTEMD_FOUND)
        set(SERVICE_TYPE "systemd")
    elseif(EXISTS "/etc/init.d")
        set(SERVICE_TYPE "sysv")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(SERVICE_TYPE "freebsd_rc")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    set(SERVICE_TYPE "bsd_rc")
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(SERVICE_TYPE "solaris_smf")
else()
    # Default to SysV for unknown Unix systems
    if(EXISTS "/etc/init.d")
        set(SERVICE_TYPE "sysv")
    endif()
endif()

message(STATUS "Detected service management: ${SERVICE_TYPE}")

# Install appropriate service files based on detected system
if(SERVICE_TYPE STREQUAL "systemd")
    # Linux systemd
    configure_file(../services/systemd/argus.service.in
        ${CMAKE_CURRENT_BINARY_DIR}/argus.service
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/argus.service
        DESTINATION /etc/systemd/system
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )

elseif(SERVICE_TYPE STREQUAL "sysv")
    # Linux SysV init
    configure_file(../services/sysv/argus.init.in
        ${CMAKE_CURRENT_BINARY_DIR}/argus.init
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/argus.init
        DESTINATION /etc/init.d
        RENAME argus
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

elseif(SERVICE_TYPE STREQUAL "freebsd_rc")
    # FreeBSD rc.d
    configure_file(../services/freebsd/argus.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/argus
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/argus
        DESTINATION /usr/local/etc/rc.d
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

elseif(SERVICE_TYPE STREQUAL "bsd_rc")
    # Other BSD systems
    configure_file(../services/freebsd/argus.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/argus.rc
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/argus.rc
        DESTINATION /etc/rc.d
        RENAME argus
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

elseif(SERVICE_TYPE STREQUAL "solaris_smf")
    # Solaris SMF
    configure_file(../services/solaris/argus.xml.in
        ${CMAKE_CURRENT_BINARY_DIR}/argus.xml
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/argus.xml
        DESTINATION /var/svc/manifest/network
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )

endif()

# Create installation directories with proper ownership
install(CODE "
    # Create necessary runtime directories
    file(MAKE_DIRECTORY \"/var/log\")
    file(MAKE_DIRECTORY \"/var/run\")

    # Platform-specific directory creation
    if(CMAKE_SYSTEM_NAME STREQUAL \"FreeBSD\")
        file(MAKE_DIRECTORY \"/usr/local/etc\")
    endif()
")

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
            COMMAND useradd -r -s /bin/false -d /var/lib/argus argus
            RESULT_VARIABLE CREATE_USER_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )

        if(CREATE_USER_RESULT EQUAL 0)
            message(STATUS \"Created argus system user\")
        else()
            message(WARNING \"Failed to create argus user. You may need to create it manually:\")
            message(WARNING \"  sudo useradd -r -s /bin/false -d /var/lib/argus argus\")
        endif()
    else()
        message(STATUS \"User 'argus' already exists\")
    endif()

    # Create log directory
    file(MAKE_DIRECTORY \"/var/log/argus\")

    # Create runtime directory
    file(MAKE_DIRECTORY \"/var/lib/argus\")

    # Set ownership of all installed files and directories to argus user
    execute_process(
        COMMAND chown -R argus:argus \${CMAKE_INSTALL_PREFIX} /var/log/argus /var/lib/argus
        RESULT_VARIABLE CHOWN_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )

    if(CHOWN_RESULT EQUAL 0)
        message(STATUS \"Set ownership of installation and system directories to argus user\")
    else()
        message(WARNING \"Failed to set ownership. You may need to run:\")
        message(WARNING \"  sudo chown -R argus:argus \${CMAKE_INSTALL_PREFIX} /var/log/argus /var/lib/argus\")
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
        COMMAND setcap cap_net_raw+ep \${CMAKE_INSTALL_PREFIX}/bin/argus
        RESULT_VARIABLE SETCAP_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )

    if(SETCAP_RESULT EQUAL 0)
        message(STATUS \"Capabilities set successfully\")
    else()
        message(WARNING \"Failed to set capabilities. To enable ping functionality, run:\")
        message(WARNING \"  sudo setcap cap_net_raw+ep \${CMAKE_INSTALL_PREFIX}/bin/argus\")
        message(WARNING \"Or enable unprivileged ICMP sockets:\")
        message(WARNING \"  sudo sysctl -w net.ipv4.ping_group_range='0 65535'\")
    endif()

    # Service-specific post-install commands and messaging
    set(SERVICE_TYPE \"${SERVICE_TYPE}\")

    if(SERVICE_TYPE STREQUAL \"systemd\")
        execute_process(COMMAND systemctl daemon-reload
            RESULT_VARIABLE DAEMON_RELOAD_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(DAEMON_RELOAD_RESULT EQUAL 0)
            message(STATUS \"Systemd service installed successfully\")
        else()
            message(WARNING \"Failed to reload systemd daemon. Run: sudo systemctl daemon-reload\")
        endif()

    elseif(SERVICE_TYPE STREQUAL \"sysv\")
        execute_process(COMMAND chkconfig --add argus
            RESULT_VARIABLE CHKCONFIG_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(CHKCONFIG_RESULT EQUAL 0)
            message(STATUS \"SysV init script installed successfully\")
        else()
            message(WARNING \"Failed to add service to chkconfig. Run: sudo chkconfig --add argus\")
        endif()

    elseif(SERVICE_TYPE STREQUAL \"freebsd_rc\")
        message(STATUS \"FreeBSD rc.d script installed successfully\")

    elseif(SERVICE_TYPE STREQUAL \"bsd_rc\")
        message(STATUS \"BSD rc.d script installed successfully\")

    elseif(SERVICE_TYPE STREQUAL \"solaris_smf\")
        execute_process(COMMAND svccfg import /var/svc/manifest/network/argus.xml
            RESULT_VARIABLE SMF_IMPORT_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(SMF_IMPORT_RESULT EQUAL 0)
            message(STATUS \"Solaris SMF service imported successfully\")
        else()
            message(WARNING \"Failed to import SMF service. Run: sudo svccfg import /var/svc/manifest/network/argus.xml\")
        endif()
    endif()

    # Display installation summary
    message(STATUS \"\")
    message(STATUS \"=== Argus Installation Complete ===\")
    message(STATUS \"Installation directory: \${CMAKE_INSTALL_PREFIX}\")
    message(STATUS \"Executable: \${CMAKE_INSTALL_PREFIX}/bin/argus\")
    message(STATUS \"Config file: \${CMAKE_INSTALL_PREFIX}/etc/argus.json\")
    message(STATUS \"Templates: \${CMAKE_INSTALL_PREFIX}/share/templates/\")
    message(STATUS \"Service management: ${SERVICE_TYPE}\")
    message(STATUS \"\")

    # Service-specific instructions
    if(SERVICE_TYPE STREQUAL \"systemd\")
        message(STATUS \"Service commands:\")
        message(STATUS \"  Enable:  sudo systemctl enable argus\")
        message(STATUS \"  Start:   sudo systemctl start argus\")
        message(STATUS \"  Status:  sudo systemctl status argus\")
        message(STATUS \"  Reload:  sudo systemctl reload argus\")

    elseif(SERVICE_TYPE STREQUAL \"sysv\")
        message(STATUS \"Service commands:\")
        message(STATUS \"  Enable:  sudo chkconfig argus on\")
        message(STATUS \"  Start:   sudo service argus start\")
        message(STATUS \"  Status:  sudo service argus status\")
        message(STATUS \"  Reload:  sudo service argus reload\")

    elseif(SERVICE_TYPE STREQUAL \"freebsd_rc\")
        message(STATUS \"Service commands:\")
        message(STATUS \"  Enable:  sudo sysrc argus_enable=YES\")
        message(STATUS \"  Start:   sudo service argus start\")
        message(STATUS \"  Status:  sudo service argus status\")
        message(STATUS \"  Reload:  sudo service argus reload\")

    elseif(SERVICE_TYPE STREQUAL \"bsd_rc\")
        message(STATUS \"Service commands:\")
        message(STATUS \"  Enable:  Add 'argus=YES' to /etc/rc.conf\")
        message(STATUS \"  Start:   sudo /etc/rc.d/argus start\")
        message(STATUS \"  Status:  sudo /etc/rc.d/argus status\")
        message(STATUS \"  Reload:  sudo /etc/rc.d/argus reload\")

    elseif(SERVICE_TYPE STREQUAL \"solaris_smf\")
        message(STATUS \"Service commands:\")
        message(STATUS \"  Enable:  sudo svcadm enable argus\")
        message(STATUS \"  Start:   sudo svcadm enable -t argus\")
        message(STATUS \"  Status:  svcs argus\")
        message(STATUS \"  Reload:  sudo svcadm refresh argus\")

    else()
        message(STATUS \"Manual start: \${CMAKE_INSTALL_PREFIX}/bin/argus \${CMAKE_INSTALL_PREFIX}/etc/argus.json\")
    endif()

    message(STATUS \"\")
")

# Create uninstall target
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/systemd/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    @ONLY
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    COMMENT "Uninstalling Argus and cleaning up system files..."
)

