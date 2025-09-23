#pragma once

#include "types.h"
#include "monitor_state.h"
#include <httplib.h>
#include <map>
#include <memory>
#include <thread>

class web_server {
public:
    web_server(monitor_config  config, const std::map<std::string, std::shared_ptr<monitor_state>>& monitors);
    ~web_server();
    void stop();

private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    monitor_config config_; // Store by value instead of reference
    const std::map<std::string, std::shared_ptr<monitor_state>>& monitors_;

    // Cached values for performance
    mutable std::string cached_status_page_;
    mutable bool status_page_cached_;
    std::string cached_config_name_;
    
    void handle_status_request(const httplib::Request& req, httplib::Response& res) const;
    void handle_api_status_request(const httplib::Request& req, httplib::Response& res);
    std::string generate_status_page() const;
    std::string generate_json_status();

    static std::string get_status_class(monitor_status status);

    static std::string format_timestamp(const std::chrono::system_clock::time_point& timestamp);
};