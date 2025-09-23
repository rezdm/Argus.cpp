#include "web_server.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <utility>
#include <nlohmann/json.hpp>

web_server::web_server(monitor_config config, const std::map<std::string, std::shared_ptr<monitor_state>>& monitors) : config_(std::move(config)), monitors_(monitors), status_page_cached_(false) {

    // Initialize cached config name with fallback
    try {
        cached_config_name_ = config_.name.empty() ? "Argus++ Monitor" : config_.name;
    } catch (...) {
        cached_config_name_ = "Argus++ Monitor";
        spdlog::warn("Failed to access config name, using default");
    }
    
    server_ = std::make_unique<httplib::Server>();
    
    // Set up route handlers
    server_->Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        handle_status_request(req, res);
    });

    server_->Get("/api/status", [this](const httplib::Request& req, httplib::Response& res) {
        handle_api_status_request(req, res);
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

void web_server::handle_status_request(const httplib::Request& req, httplib::Response& res) const {
    spdlog::debug("HTTP request from {}: {} {}", req.remote_addr, req.method, req.path);

    const std::string response = generate_status_page();

    res.set_content(response, "text/html; charset=UTF-8");

    spdlog::trace("Served status page to {} ({} bytes)", req.remote_addr, response.length());
}

void web_server::handle_api_status_request(const httplib::Request& req, httplib::Response& res) {
    spdlog::debug("API request from {}: {} {}", req.remote_addr, req.method, req.path);

    const std::string response = generate_json_status();

    res.set_content(response, "application/json; charset=UTF-8");
    res.set_header("Access-Control-Allow-Origin", "*");

    spdlog::trace("Served JSON status to {} ({} bytes)", req.remote_addr, response.length());
}

std::string web_server::generate_status_page() const {
    // Return cached page if available
    if (status_page_cached_) {
        return cached_status_page_;
    }

    std::ostringstream html;

    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "    <title>" << cached_config_name_ << " - Network Monitor</title>\n";
    html << "    <meta charset=\"UTF-8\">\n";
    html << "    <style>\n";
    html << "        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }\n";
    html << "        .header { background-color: #2c3e50; color: white; padding: 8px 15px; border-radius: 3px; margin-bottom: 15px; }\n";
    html << "        .group { background-color: white; margin-bottom: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n";
    html << "        .group-header { background-color: #34495e; color: white; padding: 15px; border-radius: 5px 5px 0 0; font-size: 18px; font-weight: bold; }\n";
    html << "        .monitor-table { width: 100%; border-collapse: collapse; table-layout: fixed; }\n";
    html << "        .monitor-table th, .monitor-table td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }\n";
    html << "        .monitor-table th:nth-child(1), .monitor-table td:nth-child(1) { width: 20%; }\n";
    html << "        .monitor-table th:nth-child(2), .monitor-table td:nth-child(2) { width: 15%; }\n";
    html << "        .monitor-table th:nth-child(3), .monitor-table td:nth-child(3) { width: 10%; }\n";
    html << "        .monitor-table th:nth-child(4), .monitor-table td:nth-child(4) { width: 12%; }\n";
    html << "        .monitor-table th:nth-child(5), .monitor-table td:nth-child(5) { width: 12%; }\n";
    html << "        .monitor-table th:nth-child(6), .monitor-table td:nth-child(6) { width: 15%; }\n";
    html << "        .monitor-table th:nth-child(7), .monitor-table td:nth-child(7) { width: 16%; }\n";
    html << "        .monitor-table th { background-color: #ecf0f1; font-weight: bold; }\n";
    html << "        .status-ok { color: #27ae60; font-weight: bold; }\n";
    html << "        .status-warning { color: #f39c12; font-weight: bold; }\n";
    html << "        .status-error { color: #e74c3c; font-weight: bold; }\n";
    html << "        .last-updated { text-align: center; margin-top: 20px; color: #7f8c8d; font-style: italic; }\n";
    html << "        .uptime-bar { width: 100px; height: 20px; background-color: #ecf0f1; border-radius: 10px; overflow: hidden; position: relative; display: inline-block; }\n";
    html << "        .uptime-fill { height: 100%; background-color: #27ae60; transition: width 0.3s ease; }\n";
    html << "        .loading { text-align: center; padding: 20px; color: #666; }\n";
    html << "        .error { text-align: center; padding: 20px; color: #e74c3c; }\n";
    html << "    </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "    <div class=\"header\">\n";
    html << "        <h3 style=\"margin: 0; font-size: 18px;\" id=\"page-title\">" << cached_config_name_ << "</h3>\n";
    html << "    </div>\n";
    html << "\n";
    html << "    <div id=\"content\">\n";
    html << "        <div class=\"loading\">Loading monitor data...</div>\n";
    html << "    </div>\n";
    html << "\n";
    html << "    <div class=\"last-updated\" id=\"last-updated\">\n";
    html << "        Loading...\n";
    html << "    </div>\n";
    html << "\n";
    html << "    <script>\n";
    html << "        function getStatusClass(status) {\n";
    html << "            switch (status.toLowerCase()) {\n";
    html << "                case 'ok': return 'status-ok';\n";
    html << "                case 'warning': return 'status-warning';\n";
    html << "                case 'failure': return 'status-error';\n";
    html << "                default: return 'status-ok';\n";
    html << "            }\n";
    html << "        }\n";
    html << "\n";
    html << "        function escapeHtml(text) {\n";
    html << "            const div = document.createElement('div');\n";
    html << "            div.textContent = text;\n";
    html << "            return div.innerHTML;\n";
    html << "        }\n";
    html << "\n";
    html << "        function updateMonitorData() {\n";
    html << "            fetch('/api/status')\n";
    html << "                .then(response => {\n";
    html << "                    if (!response.ok) {\n";
    html << "                        throw new Error('Network response was not ok');\n";
    html << "                    }\n";
    html << "                    return response.json();\n";
    html << "                })\n";
    html << "                .then(data => {\n";
    html << "                    // Update page title\n";
    html << "                    document.getElementById('page-title').textContent = data.name;\n";
    html << "                    document.title = data.name + ' - Network Monitor';\n";
    html << "\n";
    html << "                    // Generate HTML for groups\n";
    html << "                    let html = '';\n";
    html << "                    data.groups.forEach(group => {\n";
    html << "                        html += '<div class=\"group\">';\n";
    html << "                        html += '<div class=\"group-header\">' + escapeHtml(group.name) + '</div>';\n";
    html << "                        html += '<table class=\"monitor-table\">';\n";
    html << "                        html += '<thead><tr>';\n";
    html << "                        html += '<th>Service</th><th>Host</th><th>Status</th><th>Response Time</th><th>Uptime</th><th>Last Check</th><th>Details</th>';\n";
    html << "                        html += '</tr></thead><tbody>';\n";
    html << "\n";
    html << "                        group.monitors.forEach(monitor => {\n";
    html << "                            const statusClass = getStatusClass(monitor.status);\n";
    html << "                            html += '<tr>';\n";
    html << "                            html += '<td>' + escapeHtml(monitor.service) + '</td>';\n";
    html << "                            html += '<td>' + escapeHtml(monitor.host) + '</td>';\n";
    html << "                            html += '<td class=\"' + statusClass + '\">' + escapeHtml(monitor.status) + '</td>';\n";
    html << "                            html += '<td>' + escapeHtml(monitor.response_time) + '</td>';\n";
    html << "                            html += '<td>';\n";
    html << "                            html += '<div class=\"uptime-bar\">';\n";
    html << "                            html += '<div class=\"uptime-fill\" style=\"width: ' + monitor.uptime_percent + '%\"></div>';\n";
    html << "                            html += '</div> ' + monitor.uptime_percent.toFixed(1) + '%';\n";
    html << "                            html += '</td>';\n";
    html << "                            html += '<td>' + escapeHtml(monitor.last_check) + '</td>';\n";
    html << "                            html += '<td>' + escapeHtml(monitor.details) + '</td>';\n";
    html << "                            html += '</tr>';\n";
    html << "                        });\n";
    html << "\n";
    html << "                        html += '</tbody></table></div>';\n";
    html << "                    });\n";
    html << "\n";
    html << "                    document.getElementById('content').innerHTML = html;\n";
    html << "\n";
    html << "                    // Update timestamp\n";
    html << "                    document.getElementById('last-updated').textContent = \n";
    html << "                        'Last updated: ' + data.timestamp + ' | Auto-refresh every 30 seconds';\n";
    html << "                })\n";
    html << "                .catch(error => {\n";
    html << "                    console.error('Error fetching monitor data:', error);\n";
    html << "                    document.getElementById('content').innerHTML = \n";
    html << "                        '<div class=\"error\">Error loading monitor data. Please try refreshing the page.</div>';\n";
    html << "                });\n";
    html << "        }\n";
    html << "\n";
    html << "        // Initial load\n";
    html << "        updateMonitorData();\n";
    html << "\n";
    html << "        // Auto-refresh every 30 seconds\n";
    html << "        setInterval(updateMonitorData, 30000);\n";
    html << "    </script>\n";
    html << "</body>\n";
    html << "</html>\n";

    // Cache the generated page
    cached_status_page_ = html.str();
    status_page_cached_ = true;

    return cached_status_page_;
}

std::string web_server::generate_json_status() {
    using json = nlohmann::json;

    try {
        json response;
        response["name"] = cached_config_name_;
        response["timestamp"] = format_timestamp(std::chrono::system_clock::now());
        response["groups"] = json::array();

        // Group monitors by group name
        std::map<std::string, std::vector<std::shared_ptr<monitor_state>>> grouped_monitors;
        for (const auto &state: monitors_ | std::views::values) {
            if (state) { // Safety check for null pointer
                grouped_monitors[state->get_group_name()].push_back(state);
            }
        }

        // Sort groups and monitors
        std::vector<std::pair<std::string, std::vector<std::shared_ptr<monitor_state>>>> sorted_groups(
            grouped_monitors.begin(), grouped_monitors.end());

        for (auto& [group_name, states] : sorted_groups) {
            // Sort monitors within group by destination sort order
            std::ranges::sort(states, [](const auto& a, const auto& b) {
                if (!a || !b) return false;
                return a->get_destination().sort < b->get_destination().sort;
            });

            json group_obj;
            group_obj["name"] = group_name.empty() ? "Unknown Group" : group_name;
            group_obj["monitors"] = json::array();

            for (const auto& state : states) {
                if (!state) continue; // Skip null states

                try {
                    const std::string status_text = to_string(state->get_current_status());
                    const auto* last_result = state->get_last_result();
                    const std::string last_check = last_result ? format_timestamp(last_result->timestamp) : "Never";
                    const std::string response_time = last_result ? std::to_string(last_result->duration_ms) + "ms" : "N/A";
                    const double uptime_percent = state->get_uptime_percentage();
                    const std::string test_details = state->get_test_description();
                    const std::string host = state->get_destination().test.host.value_or("N/A");
                    const std::string service_name = state->get_destination().name.empty() ? "Unknown Service" : state->get_destination().name;

                    json monitor_obj;
                    monitor_obj["service"] = service_name;
                    monitor_obj["host"] = host;
                    monitor_obj["status"] = status_text;
                    monitor_obj["response_time"] = response_time;
                    monitor_obj["response_time_ms"] = last_result ? last_result->duration_ms : -1;
                    monitor_obj["uptime_percent"] = uptime_percent;
                    monitor_obj["last_check"] = last_check;
                    monitor_obj["details"] = test_details;

                    group_obj["monitors"].push_back(monitor_obj);
                } catch (const std::exception& e) {
                    spdlog::error("Error serializing monitor state: {}", e.what());
                    json error_obj;
                    error_obj["error"] = "Error serializing monitor data";
                    group_obj["monitors"].push_back(error_obj);
                }
            }

            response["groups"].push_back(group_obj);
        }

        return response.dump(2); // Pretty print with 2-space indentation
    } catch (const std::exception& e) {
        spdlog::error("Error generating JSON status: {}", e.what());
        json error_response;
        error_response["error"] = "Error generating monitor data";
        return error_response.dump(2);
    }
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

