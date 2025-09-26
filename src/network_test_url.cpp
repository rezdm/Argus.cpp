#include "network_test_url.h"
#include "http_client.h"
#include "test_config.h"
#include "test_result.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <optional>

test_result network_test_url::execute(const test_config& config, const int timeout_ms) const {
    const auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    std::string error;

    try {
        validate_config(config);

        // Validate timeout
        if (timeout_ms <= 0 || timeout_ms > 300000) { // Max 5 minutes
            throw std::invalid_argument("Invalid timeout: must be between 1ms and 300000ms");
        }

        success = perform_http_request(config.get_url().value(), config.get_proxy().value_or(""), timeout_ms);
    } catch (const std::exception& e) {
        error = e.what();
        std::string url_str = config.get_url().value_or("unknown");
        spdlog::debug("URL test failed for {}: {}", url_str, error);
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return {success, duration, std::chrono::system_clock::now(), error.empty() ? std::nullopt : std::optional(error)};
}

std::string network_test_url::get_description(const test_config& config) const {
    std::string desc = "URL: " + config.get_url().value_or("unknown");
    if (config.get_proxy() && !config.get_proxy()->empty()) {
        desc += " (via proxy)";
    }
    return desc;
}

void network_test_url::validate_config(const test_config& config) const {
    if (!config.get_url() || config.get_url()->empty()) {
        throw std::invalid_argument("URL is required for URL test");
    }
    
    // Basic URL validation - check if it starts with http:// or https://
    const std::string& url = config.get_url().value();
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        throw std::invalid_argument("Invalid URL format: " + url);
    }
}

bool network_test_url::perform_http_request(const std::string& url, const std::string& proxy, const int timeout_ms) {
    try {
        // Parse URL to extract scheme, host and path
        std::string host, path;

        const size_t scheme_end = url.find("://");
        if (scheme_end == std::string::npos) {
            return false;
        }

        const std::string scheme = url.substr(0, scheme_end);
        const size_t host_start = scheme_end + 3;

        if (const size_t path_start = url.find('/', host_start); path_start == std::string::npos) {
            host = url.substr(host_start);
            path = "/";
        } else {
            host = url.substr(host_start, path_start - host_start);
            path = url.substr(path_start);
        }

        // Create appropriate HTTP client using factory pattern
        const auto client = http_client_factory::create(scheme);
        const auto result = client->perform_request(host, path, timeout_ms, proxy);

        return result.is_success();

    } catch (const std::exception& e) {
        spdlog::debug("Exception in HTTP request for {}: {}", url, e.what());
        return false;
    }
}

