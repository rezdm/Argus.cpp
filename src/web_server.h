#pragma once

#include "types.h"
#include "monitor_state.h"
#include "thread_pool.h"
#include <httplib.h>
#include <map>
#include <memory>
#include <thread>

class web_server {
public:
    web_server(monitor_config  config, const std::map<std::string, std::shared_ptr<monitor_state>>& monitors,
               std::shared_ptr<thread_pool> pool = nullptr);
    ~web_server();
    void stop();

private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    monitor_config config_; // Store by value instead of reference
    const std::map<std::string, std::shared_ptr<monitor_state>>& monitors_;
    std::shared_ptr<thread_pool> thread_pool_;

    // Cached values for performance
    mutable std::string cached_json_status_;
    mutable bool json_status_cached_;
    mutable std::chrono::steady_clock::time_point last_cache_time_;
    std::string cached_config_name_;

    // Static HTML content (generated once)
    std::string static_html_page_;

    // Cache configuration (configurable from config file)
    std::chrono::seconds cache_duration_;
    
    void handle_status_request(const httplib::Request& req, httplib::Response& res) const;
    void handle_api_status_request(const httplib::Request& req, httplib::Response& res) const;
    void generate_static_html_page();
    std::string generate_json_status() const;

    // Template loading methods
    static std::string load_html_template_from_file(const std::string& template_path);

    // Cache management
    [[nodiscard]] bool is_json_cache_valid() const;
    void invalidate_json_cache() const;

    static std::string get_status_class(monitor_status status);

    static std::string format_timestamp(const std::chrono::system_clock::time_point& timestamp);
};