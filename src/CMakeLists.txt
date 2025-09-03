cmake_minimum_required(VERSION 3.20)
project(ArgusPlus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Find spdlog
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# Find httplib (cpp-httplib)
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.14.3
    )
    FetchContent_MakeAvailable(httplib)
endif()

# Add executable
add_executable(argus_plus
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
)

# Link libraries
target_link_libraries(argus_plus 
    PRIVATE 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    httplib::httplib
    Threads::Threads
)

# Compiler-specific options
if(MSVC)
    target_compile_options(argus_plus PRIVATE /W4)
else()
    target_compile_options(argus_plus PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set output directory
set_target_properties(argus_plus PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)