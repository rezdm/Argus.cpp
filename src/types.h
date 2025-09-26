#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <sstream>

enum class test_method {
    ping,
    connect,
    url
};

enum class protocol {
    tcp,
    udp
};

enum class monitor_status {
    ok,
    warning,
    failure
};


class test_config {
private:
    test_method test_method_type_;
    std::optional<protocol> protocol_type_;
    int port_;
    std::optional<std::string> url_;
    std::optional<std::string> proxy_;
    std::optional<std::string> host_;

public:

    // Constructors
    test_config() : test_method_type_(test_method::ping), port_(-1) {}

    explicit test_config(test_method method)
        : test_method_type_(method), port_(-1) {}

    test_config(test_method method, protocol proto, int port_val)
        : test_method_type_(method), protocol_type_(proto), port_(port_val) {}

    test_config(test_method method, const std::string& url_val)
        : test_method_type_(method), port_(-1), url_(url_val) {}

    // Getters
    [[nodiscard]] test_method get_test_method() const { return test_method_type_; }
    [[nodiscard]] const std::optional<protocol>& get_protocol() const { return protocol_type_; }
    [[nodiscard]] int get_port() const { return port_; }
    [[nodiscard]] const std::optional<std::string>& get_url() const { return url_; }
    [[nodiscard]] const std::optional<std::string>& get_proxy() const { return proxy_; }
    [[nodiscard]] const std::optional<std::string>& get_host() const { return host_; }

    // Setters with validation
    void set_test_method(test_method method) { test_method_type_ = method; }

    void set_protocol(protocol proto) { protocol_type_ = proto; }
    void clear_protocol() { protocol_type_.reset(); }

    void set_port(int port_val) {
        if (port_val < -1 || port_val > 65535) {
            throw std::invalid_argument("Port must be -1 (unset) or between 0-65535");
        }
        port_ = port_val;
    }

    void set_url(const std::string& url_val) {
        if (url_val.empty()) {
            throw std::invalid_argument("URL cannot be empty");
        }
        url_ = url_val;
    }
    void clear_url() { url_.reset(); }

    void set_proxy(const std::string& proxy_val) { proxy_ = proxy_val; }
    void clear_proxy() { proxy_.reset(); }

    void set_host(const std::string& host_val) {
        if (host_val.empty()) {
            throw std::invalid_argument("Host cannot be empty");
        }
        host_ = host_val;
    }
    void clear_host() { host_.reset(); }

    // Validation methods
    [[nodiscard]] bool is_valid() const {
        switch (test_method_type_) {
            case test_method::ping:
                return host_.has_value();
            case test_method::connect:
                return host_.has_value() && port_ > 0 && protocol_type_.has_value();
            case test_method::url:
                return url_.has_value();
        }
        return false;
    }

    [[nodiscard]] std::string get_validation_error() const {
        if (is_valid()) return "";

        switch (test_method_type_) {
            case test_method::ping:
                if (!host_.has_value()) return "Ping test requires a host";
                break;
            case test_method::connect:
                if (!host_.has_value()) return "Connect test requires a host";
                if (port_ <= 0) return "Connect test requires a valid port (1-65535)";
                if (!protocol_type_.has_value()) return "Connect test requires a protocol";
                break;
            case test_method::url:
                if (!url_.has_value()) return "URL test requires a URL";
                break;
        }
        return "Unknown validation error";
    }
};

class destination {
private:
    int sort_{};
    std::string name_;
    int timeout_{};
    int warning_{};
    int failure_{};
    int reset_{};
    int interval_{};
    int history_{};
    test_config test_;

public:

    // Constructors
    destination() = default;

    destination(int sort_val, const std::string& name_val, int timeout_val, int warning_val,
                int failure_val, int reset_val, int interval_val, int history_val, test_config test_val)
        : sort_(sort_val), name_(name_val), timeout_(timeout_val), warning_(warning_val),
          failure_(failure_val), reset_(reset_val), interval_(interval_val), history_(history_val), test_(test_val) {
        validate_parameters();
    }

    // Getters
    [[nodiscard]] int get_sort() const { return sort_; }
    [[nodiscard]] const std::string& get_name() const { return name_; }
    [[nodiscard]] int get_timeout() const { return timeout_; }
    [[nodiscard]] int get_warning() const { return warning_; }
    [[nodiscard]] int get_failure() const { return failure_; }
    [[nodiscard]] int get_reset() const { return reset_; }
    [[nodiscard]] int get_interval() const { return interval_; }
    [[nodiscard]] int get_history() const { return history_; }
    [[nodiscard]] const test_config& get_test() const { return test_; }

    // Setters with validation
    void set_sort(int sort_val) { sort_ = sort_val; }

    void set_name(const std::string& name_val) {
        if (name_val.empty()) {
            throw std::invalid_argument("Destination name cannot be empty");
        }
        name_ = name_val;
    }

    void set_timeout(int timeout_val) {
        if (timeout_val <= 0) {
            throw std::invalid_argument("Timeout must be positive");
        }
        timeout_ = timeout_val;
    }

    void set_warning(int warning_val) {
        if (warning_val <= 0) {
            throw std::invalid_argument("Warning threshold must be positive");
        }
        warning_ = warning_val;
    }

    void set_failure(int failure_val) {
        if (failure_val <= 0) {
            throw std::invalid_argument("Failure threshold must be positive");
        }
        failure_ = failure_val;
    }

    void set_reset(int reset_val) {
        if (reset_val <= 0) {
            throw std::invalid_argument("Reset threshold must be positive");
        }
        reset_ = reset_val;
    }

    void set_interval(int interval_val) {
        if (interval_val <= 0) {
            throw std::invalid_argument("Interval must be positive");
        }
        interval_ = interval_val;
    }

    void set_history(int history_val) {
        if (history_val <= 0) {
            throw std::invalid_argument("History size must be positive");
        }
        history_ = history_val;
    }

    void set_test(const test_config& test_val) {
        if (!test_val.is_valid()) {
            throw std::invalid_argument("Test configuration is invalid: " + test_val.get_validation_error());
        }
        test_ = test_val;
    }

    // Validation methods
    [[nodiscard]] bool is_valid() const {
        return !name_.empty() && timeout_ > 0 && warning_ > 0 && failure_ > 0 &&
               reset_ > 0 && interval_ > 0 && history_ > 0 && test_.is_valid();
    }

    [[nodiscard]] std::string get_validation_error() const {
        if (name_.empty()) return "Destination name cannot be empty";
        if (timeout_ <= 0) return "Timeout must be positive";
        if (warning_ <= 0) return "Warning threshold must be positive";
        if (failure_ <= 0) return "Failure threshold must be positive";
        if (reset_ <= 0) return "Reset threshold must be positive";
        if (interval_ <= 0) return "Interval must be positive";
        if (history_ <= 0) return "History size must be positive";
        if (!test_.is_valid()) return "Test configuration is invalid: " + test_.get_validation_error();
        return "";
    }

private:
    void validate_parameters() const {
        if (!is_valid()) {
            throw std::invalid_argument("Invalid destination parameters: " + get_validation_error());
        }
    }
};

class group {
private:
    int sort_;
    std::string group_name_;
    std::vector<destination> destinations_;

public:
    // Constructors
    group() : sort_(0) {}

    group(int sort_val, const std::string& group_name_val, std::vector<destination> destinations_val)
        : sort_(sort_val), group_name_(group_name_val), destinations_(std::move(destinations_val)) {
        validate_parameters();
    }

    // Getters
    [[nodiscard]] int get_sort() const { return sort_; }
    [[nodiscard]] const std::string& get_group_name() const { return group_name_; }
    [[nodiscard]] const std::vector<destination>& get_destinations() const { return destinations_; }
    [[nodiscard]] size_t get_destination_count() const { return destinations_.size(); }

    // Setters with validation
    void set_sort(int sort_val) { sort_ = sort_val; }

    void set_group_name(const std::string& group_name_val) {
        if (group_name_val.empty()) {
            throw std::invalid_argument("Group name cannot be empty");
        }
        group_name_ = group_name_val;
    }

    void set_destinations(const std::vector<destination>& destinations_val) {
        for (const auto& dest : destinations_val) {
            if (!dest.is_valid()) {
                throw std::invalid_argument("Invalid destination in group: " + dest.get_validation_error());
            }
        }
        destinations_ = destinations_val;
    }

    void add_destination(const destination& dest) {
        if (!dest.is_valid()) {
            throw std::invalid_argument("Cannot add invalid destination: " + dest.get_validation_error());
        }
        destinations_.push_back(dest);
    }

    void clear_destinations() { destinations_.clear(); }

    // Validation methods
    [[nodiscard]] bool is_valid() const {
        if (group_name_.empty()) return false;
        for (const auto& dest : destinations_) {
            if (!dest.is_valid()) return false;
        }
        return true;
    }

    [[nodiscard]] std::string get_validation_error() const {
        if (group_name_.empty()) return "Group name cannot be empty";
        for (size_t i = 0; i < destinations_.size(); ++i) {
            if (!destinations_[i].is_valid()) {
                return "Destination " + std::to_string(i) + " is invalid: " + destinations_[i].get_validation_error();
            }
        }
        return "";
    }

private:
    void validate_parameters() const {
        if (!is_valid()) {
            throw std::invalid_argument("Invalid group parameters: " + get_validation_error());
        }
    }
};

class monitor_config {
private:
    std::string name_;
    std::string listen_;
    std::optional<std::string> log_file_;
    int cache_duration_seconds_;
    std::optional<std::string> html_template_;
    size_t thread_pool_size_;
    std::vector<group> monitors_;

public:
    // Constructors
    monitor_config()
        : cache_duration_seconds_(30), thread_pool_size_(0) {}

    monitor_config(const std::string& name_val, const std::string& listen_val)
        : name_(name_val), listen_(listen_val), cache_duration_seconds_(30), thread_pool_size_(0) {
        validate_parameters();
    }

    // Getters
    [[nodiscard]] const std::string& get_name() const { return name_; }
    [[nodiscard]] const std::string& get_listen() const { return listen_; }
    [[nodiscard]] const std::optional<std::string>& get_log_file() const { return log_file_; }
    [[nodiscard]] int get_cache_duration_seconds() const { return cache_duration_seconds_; }
    [[nodiscard]] const std::optional<std::string>& get_html_template() const { return html_template_; }
    [[nodiscard]] size_t get_thread_pool_size() const { return thread_pool_size_; }
    [[nodiscard]] const std::vector<group>& get_monitors() const { return monitors_; }
    [[nodiscard]] size_t get_monitor_count() const { return monitors_.size(); }

    // Setters with validation
    void set_name(const std::string& name_val) {
        if (name_val.empty()) {
            throw std::invalid_argument("Monitor config name cannot be empty");
        }
        name_ = name_val;
    }

    void set_listen(const std::string& listen_val) {
        if (listen_val.empty()) {
            throw std::invalid_argument("Listen address cannot be empty");
        }
        listen_ = listen_val;
    }

    void set_log_file(const std::string& log_file_val) { log_file_ = log_file_val; }
    void clear_log_file() { log_file_.reset(); }

    void set_cache_duration_seconds(int duration) {
        if (duration < 0) {
            throw std::invalid_argument("Cache duration cannot be negative");
        }
        cache_duration_seconds_ = duration;
    }

    void set_html_template(const std::string& template_val) { html_template_ = template_val; }
    void clear_html_template() { html_template_.reset(); }

    void set_thread_pool_size(size_t size) { thread_pool_size_ = size; }

    void set_monitors(const std::vector<group>& monitors_val) {
        for (const auto& monitor_group : monitors_val) {
            if (!monitor_group.is_valid()) {
                throw std::invalid_argument("Invalid monitor group: " + monitor_group.get_validation_error());
            }
        }
        monitors_ = monitors_val;
    }

    void add_monitor_group(const group& monitor_group) {
        if (!monitor_group.is_valid()) {
            throw std::invalid_argument("Cannot add invalid monitor group: " + monitor_group.get_validation_error());
        }
        monitors_.push_back(monitor_group);
    }

    void clear_monitors() { monitors_.clear(); }

    // Validation methods
    [[nodiscard]] bool is_valid() const {
        if (name_.empty() || listen_.empty()) return false;
        if (cache_duration_seconds_ < 0) return false;
        for (const auto& monitor_group : monitors_) {
            if (!monitor_group.is_valid()) return false;
        }
        return true;
    }

    [[nodiscard]] std::string get_validation_error() const {
        if (name_.empty()) return "Monitor config name cannot be empty";
        if (listen_.empty()) return "Listen address cannot be empty";
        if (cache_duration_seconds_ < 0) return "Cache duration cannot be negative";
        for (size_t i = 0; i < monitors_.size(); ++i) {
            if (!monitors_[i].is_valid()) {
                return "Monitor group " + std::to_string(i) + " is invalid: " + monitors_[i].get_validation_error();
            }
        }
        return "";
    }

    // Static factory method (keeping for backward compatibility)
    static monitor_config load_config(const std::string& config_path);

private:
    void validate_parameters() const {
        if (!is_valid()) {
            throw std::invalid_argument("Invalid monitor config parameters: " + get_validation_error());
        }
    }
};

class test_result {
private:
    bool success_;
    long duration_ms_;
    std::chrono::system_clock::time_point timestamp_;
    std::optional<std::string> error_;

public:
    // Constructors
    test_result(bool success_val, long duration_ms_val,
                std::chrono::system_clock::time_point timestamp_val,
                const std::optional<std::string>& error_val = std::nullopt)
        : success_(success_val), duration_ms_(duration_ms_val),
          timestamp_(timestamp_val), error_(error_val) {
        validate_parameters();
    }

    // Convenience constructors
    static test_result create_success(long duration_ms_val) {
        return test_result(true, duration_ms_val, std::chrono::system_clock::now());
    }

    static test_result create_failure(const std::string& error_msg, long duration_ms_val = 0) {
        return test_result(false, duration_ms_val, std::chrono::system_clock::now(), error_msg);
    }

    // Getters
    [[nodiscard]] bool is_success() const { return success_; }
    [[nodiscard]] bool is_failure() const { return !success_; }
    [[nodiscard]] long get_duration_ms() const { return duration_ms_; }
    [[nodiscard]] const std::chrono::system_clock::time_point& get_timestamp() const { return timestamp_; }
    [[nodiscard]] const std::optional<std::string>& get_error() const { return error_; }
    [[nodiscard]] bool has_error() const { return error_.has_value(); }

    // Setters with validation
    void set_success(bool success_val) { success_ = success_val; }

    void set_duration_ms(long duration_ms_val) {
        if (duration_ms_val < 0) {
            throw std::invalid_argument("Duration cannot be negative");
        }
        duration_ms_ = duration_ms_val;
    }

    void set_timestamp(const std::chrono::system_clock::time_point& timestamp_val) {
        timestamp_ = timestamp_val;
    }

    void set_error(const std::string& error_val) { error_ = error_val; }
    void clear_error() { error_.reset(); }

    // Validation methods
    [[nodiscard]] bool is_valid() const {
        return duration_ms_ >= 0;
    }

    [[nodiscard]] std::string get_validation_error() const {
        if (duration_ms_ < 0) return "Duration cannot be negative";
        return "";
    }

    // Utility methods
    [[nodiscard]] std::string to_string() const {
        std::ostringstream oss;
        oss << "TestResult{success=" << success_
            << ", duration=" << duration_ms_ << "ms";
        if (error_.has_value()) {
            oss << ", error='" << error_.value() << "'";
        }
        oss << "}";
        return oss.str();
    }

private:
    void validate_parameters() const {
        if (!is_valid()) {
            throw std::invalid_argument("Invalid test result parameters: " + get_validation_error());
        }
    }
};

// String conversion functions
std::string to_string(test_method method);
std::string to_string(protocol proto);
std::string to_string(monitor_status status);
test_method parse_test_method(const std::string& str);
protocol parse_protocol(const std::string& str);