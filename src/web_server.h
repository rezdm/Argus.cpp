#pragma once

#include "types.h"
#include "monitor_state.h"
#include <httplib.h>
#include <map>
#include <memory>
#include <thread>

class web_server {
public:
    web_server(const monitor_config& config, 
               const std::map<std::string, std::shared_ptr<monitor_state>>& monitors);
    ~web_server();
    
    void stop();

private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    monitor_config config_; // Store by value instead of reference
    const std::map<std::string, std::shared_ptr<monitor_state>>& monitors_;
    
    void handle_status_request(const httplib::Request& req, httplib::Response& res);
    std::string generate_status_page();
    std::string get_status_class(monitor_status status);
    std::string format_timestamp(const std::chrono::system_clock::time_point& timestamp);
};