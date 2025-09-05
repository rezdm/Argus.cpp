#include "network_test_url.h"
#include <spdlog/spdlog.h>
#include <httplib.h>
#include <chrono>

test_result network_test_url::execute(const test_config& config, int timeout_ms) {
    const auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    std::string error;

    try {
        validate_config(config);
        success = perform_http_request(config.url.value(), 
                                     config.proxy.value_or(""), 
                                     timeout_ms);
    } catch (const std::exception& e) {
        error = e.what();
        std::string url_str = config.url.value_or("unknown");
        spdlog::debug("URL test failed for {}: {}", url_str, error);
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return {success, duration, std::chrono::system_clock::now(),
                      error.empty() ? std::nullopt : std::optional(error)};
}

std::string network_test_url::get_description(const test_config& config) {
    std::string desc = "URL: " + config.url.value_or("unknown");
    if (config.proxy && !config.proxy->empty()) {
        desc += " (via proxy)";
    }
    return desc;
}

void network_test_url::validate_config(const test_config& config) {
    if (!config.url || config.url->empty()) {
        throw std::invalid_argument("URL is required for URL test");
    }
    
    // Basic URL validation - check if it starts with http:// or https://
    const std::string& url = config.url.value();
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        throw std::invalid_argument("Invalid URL format: " + url);
    }
}

bool network_test_url::perform_http_request(const std::string& url, const std::string& proxy, int timeout_ms) {
    // Note: proxy parameter currently unused - could be implemented later
    (void)proxy; // Suppress warning
    
    try {
        // Parse URL to extract host and path
        std::string scheme, host, path;
        int port = 80;

        const size_t scheme_end = url.find("://");
        if (scheme_end == std::string::npos) {
            return false;
        }
        
        scheme = url.substr(0, scheme_end);
        if (scheme == "https") {
            port = 443;
        }

        const size_t host_start = scheme_end + 3;
        const size_t path_start = url.find('/', host_start);
        
        if (path_start == std::string::npos) {
            host = url.substr(host_start);
            path = "/";
        } else {
            host = url.substr(host_start, path_start - host_start);
            path = url.substr(path_start);
        }
        
        // Check for port in host
        const size_t port_pos = host.find(':');
        if (port_pos != std::string::npos) {
            port = std::stoi(host.substr(port_pos + 1));
            host = host.substr(0, port_pos);
        }
        
        // Set user agent
        const httplib::Headers headers = {
            {"User-Agent", "Argus++/1.0 (Network Monitor)"},
            {"Accept", "*/*"},
            {"Connection", "close"}
        };
        
        httplib::Result result;
        
        // Create appropriate client and perform request
        if (scheme == "https") {
            httplib::SSLClient ssl_client(host, port);
            ssl_client.set_connection_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
            ssl_client.set_read_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
            result = ssl_client.Get(path, headers);
        } else {
            httplib::Client http_client(host, port);
            http_client.set_connection_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
            http_client.set_read_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);
            result = http_client.Get(path, headers);
        }
        
        if (!result) {
            spdlog::debug("HTTP request failed for {}: Connection error", url);
            return false;
        }
        
        // Consider 2xx status codes as success
        const bool success = result->status >= 200 && result->status < 300;
        
        if (!success) {
            spdlog::debug("URL test failed for {}: HTTP {}", url, result->status);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        spdlog::debug("Exception in HTTP request for {}: {}", url, e.what());
        return false;
    }
}

