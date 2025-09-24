#pragma once

#include "types.h"
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

struct resolved_address {
    int family;              // AF_INET or AF_INET6
    int socktype;            // SOCK_STREAM, SOCK_DGRAM, etc.
    int protocol;            // IPPROTO_TCP, IPPROTO_UDP, etc.
    sockaddr_storage addr;   // Address data
    socklen_t addr_len;      // Address length
    std::string display_name; // For logging: "192.168.1.1" or "2001:db8::1"
};

class address_family_handler_base {
public:
    virtual ~address_family_handler_base() = default;

    // Resolve hostname to addresses for this address family
    virtual std::vector<resolved_address> resolve_addresses(const std::string& host, int port, int socktype) = 0;

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
};

class ipv4_handler final : public address_family_handler_base {
public:
    std::vector<resolved_address> resolve_addresses(const std::string& host, int port, int socktype) override;
    int create_socket(const resolved_address& addr) override;
    bool configure_socket(int socket, int timeout_ms) override;
    [[nodiscard]] std::string get_family_name() const override { return "IPv4"; }
    [[nodiscard]] int get_family_constant() const override { return AF_INET; }
};

class ipv6_handler final : public address_family_handler_base {
public:
    std::vector<resolved_address> resolve_addresses(const std::string& host, int port, int socktype) override;
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

    // Get handlers in preference order
    [[nodiscard]] std::vector<std::unique_ptr<address_family_handler_base>> get_handlers_by_preference() const;

private:
    address_family_preference preference_;
};

class address_family_factory {
public:
    static std::unique_ptr<address_family_handler_base> create_ipv4_handler();
    static std::unique_ptr<address_family_handler_base> create_ipv6_handler();
    static std::unique_ptr<address_resolver> create_resolver(address_family_preference pref);
};