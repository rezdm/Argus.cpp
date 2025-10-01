#include "address_family_handler.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <cstring>
#include <unordered_map>

#include "../core/constants.h"
#include "../core/logging.h"

// Platform-specific algorithm include
#ifdef __FreeBSD__
#include <algorithm>
#elif defined(__linux__)
#include <algorithm>
#else
#include <algorithm>
#endif

// Base class helper methods
std::string address_family_handler_base::sockaddr_to_string(const sockaddr_storage& addr) {
  char buffer[INET6_ADDRSTRLEN];

  if (addr.ss_family == AF_INET) {
    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&addr);
    if (inet_ntop(AF_INET, &ipv4->sin_addr, buffer, INET_ADDRSTRLEN) == nullptr) {
      spdlog::debug("Failed to convert IPv4 address to string: {}", strerror(errno));
      return "invalid_ipv4";
    }
  } else if (addr.ss_family == AF_INET6) {
    const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&addr);
    if (inet_ntop(AF_INET6, &ipv6->sin6_addr, buffer, INET6_ADDRSTRLEN) == nullptr) {
      spdlog::debug("Failed to convert IPv6 address to string: {}", strerror(errno));
      return "invalid_ipv6";
    }
  } else {
    spdlog::debug("Unknown address family: {}", addr.ss_family);
    return "unknown_family";
  }

  return std::string(buffer);
}

bool address_family_handler_base::set_socket_timeouts(const int socket, const int timeout_ms) {
  timeval tv{};
  tv.tv_sec = timeout_ms / argus::constants::MILLISECONDS_PER_SECOND;
  tv.tv_usec = (timeout_ms % argus::constants::MILLISECONDS_PER_SECOND) * argus::constants::MICROSECONDS_PER_MILLISECOND;

  if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
    spdlog::debug("Failed to set send timeout: {}", strerror(errno));
    return false;
  }

  if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    spdlog::debug("Failed to set receive timeout: {}", strerror(errno));
    return false;
  }

  return true;
}

resolution_error_type address_family_handler_base::classify_getaddrinfo_error(const int error_code) {
  switch (error_code) {
    case 0:
      return resolution_error_type::success;
    case EAI_NONAME:
    case EAI_NODATA:
      return resolution_error_type::dns_failure;
    case EAI_FAMILY:
      return resolution_error_type::unsupported_family;
    case EAI_AGAIN:
      return resolution_error_type::timeout;
    case EAI_FAIL:
      return resolution_error_type::network_unreachable;
    default:
      return resolution_error_type::dns_failure;
  }
}

std::string address_family_handler_base::format_resolution_error(const resolution_error_type error_type, const std::string& host, const std::string& details) {
  return resolution_error_formatter::format_error(error_type, host, details);
}

// Resolution Error Formatter Implementation
std::string resolution_error_formatter::get_base_message(const resolution_error_type error_type, const std::string& host) {
  static const std::unordered_map<resolution_error_type, std::string> error_templates = {
    {resolution_error_type::dns_failure, "DNS resolution failed for {host}"},
    {resolution_error_type::no_addresses_found, "No addresses found for {host}"},
    {resolution_error_type::unsupported_family, "Unsupported address family for {host}"},
    {resolution_error_type::network_unreachable, "Network unreachable for {host}"},
    {resolution_error_type::timeout, "DNS resolution timeout for {host}"},
    {resolution_error_type::invalid_hostname, "Invalid hostname: {host}"}
  };

  if (const auto it = error_templates.find(error_type); it != error_templates.end()) {
    std::string message = it->second;
    const size_t pos = message.find("{host}");
    if (pos != std::string::npos) {
      message.replace(pos, 6, host);
    }
    return message;
  }

  return "Unknown error for " + host;
}

std::string resolution_error_formatter::format_error(const resolution_error_type error_type, const std::string& host, const std::string& details) {
  std::string base_msg = get_base_message(error_type, host);

  if (!details.empty()) {
    base_msg += " (" + details + ")";
  }

  return base_msg;
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
    return addresses;  // Return empty vector
  }

  for (const addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    if (rp->ai_family != AF_INET) continue;  // Should not happen, but be safe

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

resolution_result ipv4_handler::resolve_addresses_detailed(const std::string& host, const int port, const int socktype) {
  resolution_result result{};

  addrinfo hints{}, *addr_result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;  // IPv4 only
  hints.ai_socktype = socktype;

  const int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addr_result);
  if (status != 0) {
    result.error_type = classify_getaddrinfo_error(status);
    result.error_message = format_resolution_error(result.error_type, host, gai_strerror(status));
    LOG_NETWORK_DEBUG("IPv4 DNS resolution", host, gai_strerror(status));
    return result;
  }

  for (const addrinfo* rp = addr_result; rp != nullptr; rp = rp->ai_next) {
    if (rp->ai_family != AF_INET) continue;  // Should not happen, but be safe

    resolved_address addr{};
    addr.family = rp->ai_family;
    addr.socktype = rp->ai_socktype;
    addr.protocol = rp->ai_protocol;
    addr.addr_len = rp->ai_addrlen;

    memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);
    addr.display_name = sockaddr_to_string(addr.addr);

    result.addresses.push_back(addr);
    spdlog::trace("Resolved IPv4 address for {}: {}", host, addr.display_name);
  }

  freeaddrinfo(addr_result);

  if (result.addresses.empty()) {
    result.error_type = resolution_error_type::no_addresses_found;
    result.error_message = format_resolution_error(result.error_type, host);
  } else {
    result.error_type = resolution_error_type::success;
  }

  return result;
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

bool ipv4_handler::configure_socket(const int socket, const int timeout_ms) { return set_socket_timeouts(socket, timeout_ms); }

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
    return addresses;  // Return empty vector
  }

  for (const addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    if (rp->ai_family != AF_INET6) continue;  // Should not happen, but be safe

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

resolution_result ipv6_handler::resolve_addresses_detailed(const std::string& host, const int port, const int socktype) {
  resolution_result result{};

  addrinfo hints{}, *addr_result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;  // IPv6 only
  hints.ai_socktype = socktype;

  if (const int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addr_result); status != 0) {
    result.error_type = classify_getaddrinfo_error(status);
    result.error_message = format_resolution_error(result.error_type, host, gai_strerror(status));
    LOG_NETWORK_DEBUG("IPv6 DNS resolution", host, gai_strerror(status));
    return result;
  }

  for (const addrinfo* rp = addr_result; rp != nullptr; rp = rp->ai_next) {
    if (rp->ai_family != AF_INET6) continue;  // Should not happen, but be safe

    resolved_address addr{};
    addr.family = rp->ai_family;
    addr.socktype = rp->ai_socktype;
    addr.protocol = rp->ai_protocol;
    addr.addr_len = rp->ai_addrlen;

    memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);
    addr.display_name = sockaddr_to_string(addr.addr);

    result.addresses.push_back(addr);
    spdlog::trace("Resolved IPv6 address for {}: {}", host, addr.display_name);
  }

  freeaddrinfo(addr_result);

  if (result.addresses.empty()) {
    result.error_type = resolution_error_type::no_addresses_found;
    result.error_message = format_resolution_error(result.error_type, host);
  } else {
    result.error_type = resolution_error_type::success;
  }

  return result;
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

bool ipv6_handler::configure_socket(const int socket, const int timeout_ms) { return set_socket_timeouts(socket, timeout_ms); }

// Strategy Implementations
std::vector<std::unique_ptr<address_family_handler_base>> ipv4_only_strategy::get_handlers() const {
  std::vector<std::unique_ptr<address_family_handler_base>> handlers;
  handlers.push_back(std::make_unique<ipv4_handler>());
  return handlers;
}

std::vector<std::unique_ptr<address_family_handler_base>> ipv6_only_strategy::get_handlers() const {
  std::vector<std::unique_ptr<address_family_handler_base>> handlers;
  handlers.push_back(std::make_unique<ipv6_handler>());
  return handlers;
}

std::vector<std::unique_ptr<address_family_handler_base>> ipv6_preferred_strategy::get_handlers() const {
  std::vector<std::unique_ptr<address_family_handler_base>> handlers;
  handlers.push_back(std::make_unique<ipv6_handler>());
  handlers.push_back(std::make_unique<ipv4_handler>());
  return handlers;
}

std::vector<std::unique_ptr<address_family_handler_base>> ipv4_preferred_strategy::get_handlers() const {
  std::vector<std::unique_ptr<address_family_handler_base>> handlers;
  handlers.push_back(std::make_unique<ipv4_handler>());
  handlers.push_back(std::make_unique<ipv6_handler>());
  return handlers;
}

std::vector<std::unique_ptr<address_family_handler_base>> dual_stack_strategy::get_handlers() const {
  std::vector<std::unique_ptr<address_family_handler_base>> handlers;
  handlers.push_back(std::make_unique<ipv6_handler>());
  handlers.push_back(std::make_unique<ipv4_handler>());
  return handlers;
}

// Address Resolver Implementation
address_resolver::address_resolver(const address_family_preference preference)
  : strategy_(address_family_factory::create_strategy(preference)) {}

address_resolver::address_resolver(std::unique_ptr<address_preference_strategy> strategy)
  : strategy_(std::move(strategy)) {}

std::vector<resolved_address> address_resolver::resolve_with_preference(const std::string& host, const int port, const int socktype) const {
  std::vector<resolved_address> all_addresses;

  for (const auto handlers = strategy_->get_handlers(); const auto& handler : handlers) {
    auto addresses = handler->resolve_addresses(host, port, socktype);
    spdlog::trace("Resolved {} {} addresses for {}", addresses.size(), handler->get_family_name(), host);

    // Add to result list
    all_addresses.insert(all_addresses.end(), addresses.begin(), addresses.end());

    // For non-dual-stack modes, stop after first successful resolution
    if (!addresses.empty() && !strategy_->is_dual_stack()) {
      spdlog::debug("Using {} addresses for {}", handler->get_family_name(), host);
      break;
    }
  }

  return all_addresses;
}

std::vector<resolved_address> address_resolver::resolve_optimized(const std::string& host, const int port, const int socktype) const {
  // Quick check if this is a numeric IP address
  const auto ip_type = ip_address_utils::detect_ip_type(host);

  if (ip_type != ip_address_utils::ip_type::invalid) {
    // It's a numeric IP, use the appropriate handler directly
    std::vector<resolved_address> addresses;

    if (ip_type == ip_address_utils::ip_type::ipv4) {
      const auto handler = std::make_unique<ipv4_handler>();
      addresses = handler->resolve_addresses(host, port, socktype);
      if (!addresses.empty()) {
        spdlog::debug("Directly resolved IPv4 address: {}", host);
        return addresses;
      }
    } else if (ip_type == ip_address_utils::ip_type::ipv6) {
      const auto handler = std::make_unique<ipv6_handler>();
      addresses = handler->resolve_addresses(host, port, socktype);
      if (!addresses.empty()) {
        spdlog::debug("Directly resolved IPv6 address: {}", host);
        return addresses;
      }
    }
  }

  // Fall back to normal DNS resolution with preference
  return resolve_with_preference(host, port, socktype);
}

std::vector<std::unique_ptr<address_family_handler_base>> address_resolver::get_handlers_by_preference() const {
  return strategy_->get_handlers();
}

// Factory Implementation
std::unique_ptr<address_family_handler_base> address_family_factory::create_ipv4_handler() { return std::make_unique<ipv4_handler>(); }

std::unique_ptr<address_family_handler_base> address_family_factory::create_ipv6_handler() { return std::make_unique<ipv6_handler>(); }

std::unique_ptr<address_family_handler_base> address_family_factory::create_handler_for_family(const int family) {
  switch (family) {
    case AF_INET:
      return create_ipv4_handler();
    case AF_INET6:
      return create_ipv6_handler();
    default:
      throw std::invalid_argument("Unsupported address family: " + std::to_string(family));
  }
}

std::unique_ptr<address_preference_strategy> address_family_factory::create_strategy(const address_family_preference pref) {
  switch (pref) {
    case address_family_preference::ipv4_only:
      return std::make_unique<ipv4_only_strategy>();
    case address_family_preference::ipv6_only:
      return std::make_unique<ipv6_only_strategy>();
    case address_family_preference::ipv6_preferred:
      return std::make_unique<ipv6_preferred_strategy>();
    case address_family_preference::ipv4_preferred:
      return std::make_unique<ipv4_preferred_strategy>();
    case address_family_preference::dual_stack:
      return std::make_unique<dual_stack_strategy>();
    default:
      throw std::invalid_argument("Unknown address family preference");
  }
}

std::unique_ptr<address_resolver> address_family_factory::create_resolver(address_family_preference pref) { return std::make_unique<address_resolver>(pref); }

// IP Address Utilities Implementation
ip_address_utils::ip_type ip_address_utils::detect_ip_type(const std::string& address) {
  if (is_valid_ipv4(address)) {
    return ip_type::ipv4;
  }
  if (is_valid_ipv6(address)) {
    return ip_type::ipv6;
  }
  return ip_type::invalid;
}

bool ip_address_utils::is_valid_ipv4(const std::string& address) {
  sockaddr_in sa4{};
  return inet_pton(AF_INET, address.c_str(), &sa4.sin_addr) == 1;
}

bool ip_address_utils::is_valid_ipv6(const std::string& address) {
  sockaddr_in6 sa6{};
  return inet_pton(AF_INET6, address.c_str(), &sa6.sin6_addr) == 1;
}

bool ip_address_utils::is_numeric_ip(const std::string& address) { return detect_ip_type(address) != ip_type::invalid; }

std::string ip_address_utils::normalize_ipv6(const std::string& address) {
  sockaddr_in6 sa6{};
  if (inet_pton(AF_INET6, address.c_str(), &sa6.sin6_addr) != 1) {
    return address;  // Return original if invalid
  }

  char buffer[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET6, &sa6.sin6_addr, buffer, INET6_ADDRSTRLEN) != nullptr) {
    return std::string(buffer);
  }

  return address;  // Return original if conversion fails
}

bool ip_address_utils::is_ipv4_mapped_ipv6(const std::string& address) {
  if (!is_valid_ipv6(address)) {
    return false;
  }

  sockaddr_in6 sa6{};
  inet_pton(AF_INET6, address.c_str(), &sa6.sin6_addr);

  // Check for IPv4-mapped IPv6 address (::ffff:0:0/96)
  const auto* addr_bytes = reinterpret_cast<const uint8_t*>(&sa6.sin6_addr);
  return (addr_bytes[10] == 0xff && addr_bytes[11] == 0xff && std::all_of(addr_bytes, addr_bytes + 10, [](const uint8_t b) { return b == 0; }));
}