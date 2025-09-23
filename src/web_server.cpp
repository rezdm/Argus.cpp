#include "web_server.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <utility>

web_server::web_server(monitor_config  config,
                       const std::map<std::string, std::shared_ptr<monitor_state>>& monitors)
    : config_(std::move(config)), monitors_(monitors) {
    
    server_ = std::make_unique<httplib::Server>();
    
    // Set up route handler
    server_->Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        handle_status_request(req, res);
    });
    
    // Parse listen address
    std::string host;
    int port;
    if (const size_t colon_pos = config_.listen.find(':'); colon_pos != std::string::npos) {
        host = config_.listen.substr(0, colon_pos);
        port = std::stoi(config_.listen.substr(colon_pos + 1));
    } else {
        host = "localhost";
        port = std::stoi(config_.listen);
    }
    
    // Start server in a separate thread
    server_thread_ = std::thread([this, host, port]() {
        spdlog::info("Argus++ web server starting on {}:{}", host, port);
        if (!server_->listen(host, port)) {
            spdlog::error("Failed to start web server on {}:{}", host, port);
        }
    });
    
    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    spdlog::info("Argus++ web server started on {}", config_.listen);
}

web_server::~web_server() {
    stop();
}

void web_server::stop() {
    if (server_) {
        server_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        spdlog::info("Web server stopped");
    }
}

void web_server::handle_status_request(const httplib::Request& req, httplib::Response& res) {
    spdlog::debug("HTTP request from {}: {} {}", 
                 req.remote_addr, req.method, req.path);

    const std::string response = generate_status_page();
    
    res.set_content(response, "text/html; charset=UTF-8");
    
    spdlog::trace("Served status page to {} ({} bytes)", 
                 req.remote_addr, response.length());
}

std::string web_server::generate_status_page() {
    std::ostringstream html;
    
    // Safely access config name with fallback
    std::string config_name;
    try {
        config_name = config_.name.empty() ? "Argus++ Monitor" : config_.name;
    } catch (...) {
        config_name = "Argus++ Monitor";
        spdlog::warn("Failed to access config name, using default");
    }
    
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "    <title>" << config_name << " - Network Monitor</title>\n";
    html << "    <meta charset=\"UTF-8\">\n";
    html << "    <meta http-equiv=\"refresh\" content=\"30\">\n";
    html << "    <style>\n";
    html << "        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }\n";
    html << "        .header { background-color: #2c3e50; color: white; padding: 20px; border-radius: 5px; margin-bottom: 20px; }\n";
    html << "        .group { background-color: white; margin-bottom: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n";
    html << "        .group-header { background-color: #34495e; color: white; padding: 15px; border-radius: 5px 5px 0 0; font-size: 18px; font-weight: bold; }\n";
    html << "        .monitor-table { width: 100%; border-collapse: collapse; }\n";
    html << "        .monitor-table th, .monitor-table td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }\n";
    html << "        .monitor-table th { background-color: #ecf0f1; font-weight: bold; }\n";
    html << "        .status-ok { color: #27ae60; font-weight: bold; }\n";
    html << "        .status-warning { color: #f39c12; font-weight: bold; }\n";
    html << "        .status-error { color: #e74c3c; font-weight: bold; }\n";
    html << "        .last-updated { text-align: center; margin-top: 20px; color: #7f8c8d; font-style: italic; }\n";
    html << "        .uptime-bar { width: 100px; height: 20px; background-color: #ecf0f1; border-radius: 10px; overflow: hidden; position: relative; display: inline-block; }\n";
    html << "        .uptime-fill { height: 100%; background-color: #27ae60; transition: width 0.3s ease; }\n";
    html << "    </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "    <div class=\"header\">\n";
    html << "        <h1>" << config_name << "</h1>\n";
    html << "        <p>Network Monitoring Dashboard</p>\n";
    html << "    </div>\n";

    try {
        // Group monitors by group name
        std::map<std::string, std::vector<std::shared_ptr<monitor_state>>> grouped_monitors;
        for (const auto& [key, state] : monitors_) {
            if (state) { // Safety check for null pointer
                grouped_monitors[state->get_group_name()].push_back(state);
            }
        }
        
        // Sort groups and monitors
        std::vector<std::pair<std::string, std::vector<std::shared_ptr<monitor_state>>>> sorted_groups(
            grouped_monitors.begin(), grouped_monitors.end());
        
        for (auto& [group_name, states] : sorted_groups) {
            // Sort monitors within group by destination sort order
            std::sort(states.begin(), states.end(),
                     [](const auto& a, const auto& b) {
                         if (!a || !b) return false;
                         return a->get_destination().sort < b->get_destination().sort;
                     });
            
            html << "    <div class=\"group\">\n";
            html << "        <div class=\"group-header\">" << (group_name.empty() ? "Unknown Group" : group_name) << "</div>\n";
            html << "        <table class=\"monitor-table\">\n";
            html << "            <thead>\n";
            html << "                <tr>\n";
            html << "                    <th>Service</th>\n";
            html << "                    <th>Host</th>\n";
            html << "                    <th>Status</th>\n";
            html << "                    <th>Response Time</th>\n";
            html << "                    <th>Uptime</th>\n";
            html << "                    <th>Last Check</th>\n";
            html << "                    <th>Details</th>\n";
            html << "                </tr>\n";
            html << "            </thead>\n";
            html << "            <tbody>\n";
            
            for (const auto& state : states) {
                if (!state) continue; // Skip null states
                
                try {
                    const std::string status_class = get_status_class(state->get_current_status());
                    const std::string status_text = to_string(state->get_current_status());
                    
                    const auto* last_result = state->get_last_result();
                    const std::string last_check = last_result ? 
                        format_timestamp(last_result->timestamp) : "Never";
                    const std::string response_time = last_result ? 
                        std::to_string(last_result->duration_ms) + "ms" : "N/A";
                    
                    const double uptime_percent = state->get_uptime_percentage();
                    const std::string test_details = state->get_test_description();
                    
                    const std::string host = state->get_destination().test.host.value_or("N/A");
                    const std::string service_name = state->get_destination().name.empty() ? 
                        "Unknown Service" : state->get_destination().name;
                    
                    html << "                <tr>\n";
                    html << "                    <td>" << service_name << "</td>\n";
                    html << "                    <td>" << host << "</td>\n";
                    html << "                    <td class=\"" << status_class << "\">" << status_text << "</td>\n";
                    html << "                    <td>" << response_time << "</td>\n";
                    html << "                    <td>\n";
                    html << "                        <div class=\"uptime-bar\">\n";
                    html << R"(                            <div class="uptime-fill" style="width: )" << std::fixed << std::setprecision(1) << uptime_percent << "%\"></div>\n";
                    html << "                        </div>\n";
                    html << "                        " << std::fixed << std::setprecision(1) << uptime_percent << "%\n";
                    html << "                    </td>\n";
                    html << "                    <td>" << last_check << "</td>\n";
                    html << "                    <td>" << test_details << "</td>\n";
                    html << "                </tr>\n";
                } catch (const std::exception& e) {
                    spdlog::error("Error rendering monitor state: {}", e.what());
                    html << "                <tr><td colspan=\"7\">Error rendering monitor data</td></tr>\n";
                }
            }
            
            html << "            </tbody>\n";
            html << "        </table>\n";
            html << "    </div>\n";
        }
    } catch (const std::exception& e) {
        spdlog::error("Error generating status page: {}", e.what());
        html << "    <div class=\"group\"><p>Error generating monitor data</p></div>\n";
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    std::string current_time = format_timestamp(now);
    
    html << "    <div class=\"last-updated\">\n";
    html << "        Last updated: " << current_time << " | Auto-refresh every 30 seconds\n";
    html << "    </div>\n";
    html << "</body>\n";
    html << "</html>\n";

    return html.str();
}

std::string web_server::get_status_class(const monitor_status status) {
    switch (status) {
        case monitor_status::ok: return "status-ok";
        case monitor_status::warning: return "status-warning";
        case monitor_status::failure: return "status-error";
        default: return "status-ok";
    }
}

std::string web_server::format_timestamp(const std::chrono::system_clock::time_point& timestamp) {
    const auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

