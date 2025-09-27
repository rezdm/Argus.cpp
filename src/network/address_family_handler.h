#pragma once

#include "../core/types.h"
#include <string>
#include <memory>
#include <vector>
#include <netdb.h>

enum class address_family_preference {
    ipv4_only,
    ipv6_only,
    ipv6_preferred,  // Try IPv6 first, fallback to IPv4
    ipv4_preferred,  // Try IPv4 first, fallback to IPv6
    dual_stack       // Try both simultaneously (current behavior)
};

enum class resolution_error_type {
    success,
    dns_failure,
    no_addresses_found,
    unsupported_family,
    network_unreachable,
    timeout,
    invalid_hostname
};

struct resolved_address {
    int family;              // AF_INET or AF_INET6
    int socktype;            // SOCK_STREAM, SOCK_DGRAM, etc.
    int protocol;            // IPPROTO_TCP, IPPROTO_UDP, etc.
    sockaddr_storage addr;   // Address data
    socklen_t addr_len;      // Address length
    std::string display_name; // For logging: "192.168.1.1" or "2001:db8::1"
};

struct resolution_result {
    std::vector<resolved_address> addresses;
    resolution_error_type error_type;
    std::string error_message;

    [[nodiscard]] bool is_success() const { return error_type == resolution_error_type::success && !addresses.empty(); }
    [[nodiscard]] bool has_addresses() const { return !addresses.empty(); }
};

class address_family_handler_base {
public:
    virtual ~address_family_handler_base() = default;

    // Resolve hostname to addresses for this address family
    virtual std::vector<resolved_address> resolve_addresses(const std::string& host, int port, int socktype) = 0;

    // Resolve with detailed error information
    virtual resolution_result resolve_addresses_detailed(const std::string& host, int port, int socktype) = 0;

    // Create socket for this address family with appropriate options
    virtual int create_socket(const resolved_address& addr) = 0;

    // Set family-specific socket options (timeouts, etc.)
    virtual bool configure_socket(int socket, int timeout_ms) = 0;

    // Get display name for this address family
    [[nodiscard]] virtual std::string get_family_name() const = 0;

    // Get the address family constant
    [[nodiscard]] virtual int get_family_constant() const = 0;

protected:
    static std::string sockaddr_to_string(const sockaddr_storage& addr);
    static bool set_socket_timeouts(int socket, int timeout_ms);
    static resolution_error_type classify_getaddrinfo_error(int error_code);
    static std::string format_resolution_error(resolution_error_type error_type, const std::string& host, const std::string& details = "");
};

class ipv4_handler final : public address_family_handler_base {
public:
    std::vector<resolved_address> resolve_addresses(const std::string& host, int port, int socktype) override;
    resolution_result resolve_addresses_detailed(const std::string& host, int port, int socktype) override;
    int create_socket(const resolved_address& addr) override;
    bool configure_socket(int socket, int timeout_ms) override;
    [[nodiscard]] std::string get_family_name() const override { return "IPv4"; }
    [[nodiscard]] int get_family_constant() const override { return AF_INET; }
};

class ipv6_handler final : public address_family_handler_base {
public:
    std::vector<resolved_address> resolve_addresses(const std::string& host, int port, int socktype) override;
    resolution_result resolve_addresses_detailed(const std::string& host, int port, int socktype) override;
    int create_socket(const resolved_address& addr) override;
    bool configure_socket(int socket, int timeout_ms) override;
    [[nodiscard]] std::string get_family_name() const override { return "IPv6"; }
    [[nodiscard]] int get_family_constant() const override { return AF_INET6; }
};

class address_resolver {
public:
    explicit address_resolver(address_family_preference preference = address_family_preference::ipv6_preferred);

    // Resolve addresses according to the configured preference strategy
    std::vector<resolved_address> resolve_with_preference(const std::string& host, int port, int socktype);

    // Resolve with optimization for numeric IP addresses
    std::vector<resolved_address> resolve_optimized(const std::string& host, int port, int socktype);

    // Get handlers in preference order
    [[nodiscard]] std::vector<std::unique_ptr<address_family_handler_base>> get_handlers_by_preference() const;

private:
    address_family_preference preference_;
};

class ip_address_utils {
public:
    // Detect IP address type from string
    enum class ip_type {
        invalid,
        ipv4,
        ipv6
    };

    static ip_type detect_ip_type(const std::string& address);
    static bool is_valid_ipv4(const std::string& address);
    static bool is_valid_ipv6(const std::string& address);
    static bool is_numeric_ip(const std::string& address);

    // Convert between different address representations
    static std::string normalize_ipv6(const std::string& address);
    static bool is_ipv4_mapped_ipv6(const std::string& address);
};

class address_family_factory {
public:
    static std::unique_ptr<address_family_handler_base> create_ipv4_handler();
    static std::unique_ptr<address_family_handler_base> create_ipv6_handler();
    static std::unique_ptr<address_resolver> create_resolver(address_family_preference pref);
};