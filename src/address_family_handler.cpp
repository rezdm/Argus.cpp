#include "address_family_handler.h"
#include "constants.h"
#include "logging.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// Base class helper methods
std::string address_family_handler_base::sockaddr_to_string(const sockaddr_storage& addr) {
    char buffer[INET6_ADDRSTRLEN];

    if (addr.ss_family == AF_INET) {
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&addr);
        inet_ntop(AF_INET, &ipv4->sin_addr, buffer, INET_ADDRSTRLEN);
    } else if (addr.ss_family == AF_INET6) {
        const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&addr);
        inet_ntop(AF_INET6, &ipv6->sin6_addr, buffer, INET6_ADDRSTRLEN);
    } else {
        return "unknown";
    }

    return std::string(buffer);
}

// IPv4 Handler Implementation
std::vector<resolved_address> ipv4_handler::resolve_addresses(const std::string& host, const int port, const int socktype) {

    std::vector<resolved_address> addresses;

    addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4 only
    hints.ai_socktype = socktype;

    if (const int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result); status != 0) {
        LOG_NETWORK_DEBUG("IPv4 DNS resolution", host, gai_strerror(status));
        return addresses; // Return empty vector
    }

    for (const addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET) continue; // Should not happen, but be safe

        resolved_address addr{};
        addr.family = rp->ai_family;
        addr.socktype = rp->ai_socktype;
        addr.protocol = rp->ai_protocol;
        addr.addr_len = rp->ai_addrlen;

        memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);
        addr.display_name = sockaddr_to_string(addr.addr);

        addresses.push_back(addr);
        spdlog::trace("Resolved IPv4 address for {}: {}", host, addr.display_name);
    }

    freeaddrinfo(result);
    return addresses;
}

int ipv4_handler::create_socket(const resolved_address& addr) {
    const int sock = socket(addr.family, addr.socktype, addr.protocol);
    if (sock < 0) {
        spdlog::debug("Failed to create IPv4 socket: {}", strerror(errno));
        return -1;
    }

    // Set IPv4-specific socket options
    constexpr int opt = argus::constants::SOCKET_OPTION_ENABLE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    return sock;
}

bool ipv4_handler::configure_socket(const int socket, const int timeout_ms) {
    // Set timeout options
    timeval tv{};
    tv.tv_sec = timeout_ms / argus::constants::MILLISECONDS_PER_SECOND;
    tv.tv_usec = (timeout_ms % argus::constants::MILLISECONDS_PER_SECOND) * argus::constants::MICROSECONDS_PER_MILLISECOND;

    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        spdlog::debug("Failed to set IPv4 send timeout: {}", strerror(errno));
        return false;
    }

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        spdlog::debug("Failed to set IPv4 receive timeout: {}", strerror(errno));
        return false;
    }

    return true;
}

// IPv6 Handler Implementation
std::vector<resolved_address> ipv6_handler::resolve_addresses(const std::string& host, const int port, const int socktype) {

    std::vector<resolved_address> addresses;

    addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;  // IPv6 only
    hints.ai_socktype = socktype;

    const int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (status != 0) {
        LOG_NETWORK_DEBUG("IPv6 DNS resolution", host, gai_strerror(status));
        return addresses; // Return empty vector
    }

    for (const addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET6) continue; // Should not happen, but be safe

        resolved_address addr{};
        addr.family = rp->ai_family;
        addr.socktype = rp->ai_socktype;
        addr.protocol = rp->ai_protocol;
        addr.addr_len = rp->ai_addrlen;

        memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);
        addr.display_name = sockaddr_to_string(addr.addr);

        addresses.push_back(addr);
        spdlog::trace("Resolved IPv6 address for {}: {}", host, addr.display_name);
    }

    freeaddrinfo(result);
    return addresses;
}

int ipv6_handler::create_socket(const resolved_address& addr) {
    const int sock = socket(addr.family, addr.socktype, addr.protocol);
    if (sock < 0) {
        spdlog::debug("Failed to create IPv6 socket: {}", strerror(errno));
        return -1;
    }

    // Set IPv6-specific socket options
    constexpr int opt = argus::constants::SOCKET_OPTION_ENABLE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Ensure IPv6-only (don't accept IPv4-mapped addresses)
    setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));

    return sock;
}

bool ipv6_handler::configure_socket(const int socket, const int timeout_ms) {
    // Set timeout options (same as IPv4 for now)
    timeval tv{};
    tv.tv_sec = timeout_ms / argus::constants::MILLISECONDS_PER_SECOND;
    tv.tv_usec = (timeout_ms % argus::constants::MILLISECONDS_PER_SECOND) * argus::constants::MICROSECONDS_PER_MILLISECOND;

    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        spdlog::debug("Failed to set IPv6 send timeout: {}", strerror(errno));
        return false;
    }

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        spdlog::debug("Failed to set IPv6 receive timeout: {}", strerror(errno));
        return false;
    }

    return true;
}

// Address Resolver Implementation
address_resolver::address_resolver(const address_family_preference preference)
    : preference_(preference) {}

std::vector<resolved_address> address_resolver::resolve_with_preference(
    const std::string& host, const int port, const int socktype) {

    std::vector<resolved_address> all_addresses;

    for (const auto handlers = get_handlers_by_preference();
        const auto& handler : handlers) {
        auto addresses = handler->resolve_addresses(host, port, socktype);
        spdlog::trace("Resolved {} {} addresses for {}", addresses.size(), handler->get_family_name(), host);

        // Add to result list
        all_addresses.insert(all_addresses.end(), addresses.begin(), addresses.end());

        // For non-dual-stack modes, stop after first successful resolution
        if (!addresses.empty() && preference_ != address_family_preference::dual_stack) {
            spdlog::debug("Using {} addresses for {} (preference: {})", handler->get_family_name(), host, static_cast<int>(preference_));
            break;
        }
    }

    return all_addresses;
}

std::vector<std::unique_ptr<address_family_handler_base>> address_resolver::get_handlers_by_preference() const {
    std::vector<std::unique_ptr<address_family_handler_base>> handlers;

    switch (preference_) {
        case address_family_preference::ipv4_only:
            handlers.push_back(std::make_unique<ipv4_handler>());
            break;

        case address_family_preference::ipv6_only:
            handlers.push_back(std::make_unique<ipv6_handler>());
            break;

        case address_family_preference::ipv6_preferred:
            handlers.push_back(std::make_unique<ipv6_handler>());
            handlers.push_back(std::make_unique<ipv4_handler>());
            break;

        case address_family_preference::ipv4_preferred:
            handlers.push_back(std::make_unique<ipv4_handler>());
            handlers.push_back(std::make_unique<ipv6_handler>());
            break;

        case address_family_preference::dual_stack:
            handlers.push_back(std::make_unique<ipv6_handler>());
            handlers.push_back(std::make_unique<ipv4_handler>());
            break;
    }

    return handlers;
}

// Factory Implementation
std::unique_ptr<address_family_handler_base> address_family_factory::create_ipv4_handler() {
    return std::make_unique<ipv4_handler>();
}

std::unique_ptr<address_family_handler_base> address_family_factory::create_ipv6_handler() {
    return std::make_unique<ipv6_handler>();
}

std::unique_ptr<address_resolver> address_family_factory::create_resolver(address_family_preference pref) {
    return std::make_unique<address_resolver>(pref);
}