#pragma once

#include "types.h"
#include <string>
#include <memory>

class http_client_base {
public:
    virtual ~http_client_base() = default;
    virtual test_result perform_request(const std::string& url, const std::string& path, int timeout_ms, const std::string& proxy = "") = 0;
    [[nodiscard]] virtual std::string get_scheme() const = 0;

protected:
    static test_result create_error_result(const std::string& error_msg, long duration = 0);
    static test_result create_success_result(long duration);
};

class http_client final : public http_client_base {
public:
    test_result perform_request(const std::string& host, const std::string& path, int timeout_ms, const std::string& proxy = "") override;
    [[nodiscard]] std::string get_scheme() const override { return "http"; }

private:
    static int extract_port_from_host(std::string& host, int default_port = 80);
};

class https_client final : public http_client_base {
public:
    https_client();
    test_result perform_request(const std::string& host, const std::string& path, int timeout_ms, const std::string& proxy = "") override;
    [[nodiscard]] std::string get_scheme() const override { return "https"; }

private:
    static int extract_port_from_host(std::string& host, int default_port = 443);
    bool enable_cert_verification_;
};

class http_client_factory {
public:
    static std::unique_ptr<http_client_base> create(const std::string& scheme);
};