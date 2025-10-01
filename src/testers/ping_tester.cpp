#include "ping_tester.h"

#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <optional>

#include "../utils/test_result.h"

// Platform-specific ICMP includes
#ifdef __FreeBSD__
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#elif defined(__linux__)
#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>
#else
// Solaris and other Unix systems - need proper include order
#include <netinet/icmp6.h>
#include <netinet/in_systm.h>  // Provides n_time definition
#include <netinet/ip.h>        // Must come before ip_icmp.h
#include <netinet/ip_icmp.h>
#include <sys/types.h>
#endif

#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <random>
#include <regex>
#include <sstream>

// Helper function for checksum calculation
static uint16_t calculate_icmp_checksum(const void* data, size_t len) {
  const auto* buf = static_cast<const uint16_t*>(data);
  uint32_t sum = 0;

  while (len > 1) {
    sum += *buf++;
    len -= 2;
  }

  if (1 == len) {
    sum += *reinterpret_cast<const uint8_t*>(buf) << 8;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  return static_cast<uint16_t>(~sum);
}

// ICMP Handler Implementations

// Linux IPv4 ICMP Handler
#ifdef __linux__
std::vector<uint8_t> linux_ipv4_icmp_handler::build_echo_request(const uint16_t identifier, const uint16_t sequence) const {
  icmphdr icmp{};
  icmp.type = ICMP_ECHO;
  icmp.code = 0;
  icmp.un.echo.id = htons(identifier);
  icmp.un.echo.sequence = htons(sequence);
  icmp.checksum = 0;
  icmp.checksum = calculate_icmp_checksum(&icmp, sizeof(icmp));

  std::vector<uint8_t> packet(sizeof(icmp));
  memcpy(packet.data(), &icmp, sizeof(icmp));
  return packet;
}

bool linux_ipv4_icmp_handler::is_echo_reply_unprivileged(const void* data, const size_t len) const {
  if (len < sizeof(icmphdr)) return false;
  const auto* icmp_hdr = static_cast<const icmphdr*>(data);
  return ICMP_ECHOREPLY == icmp_hdr->type;
}

bool linux_ipv4_icmp_handler::is_echo_reply_raw(const void* data, const size_t len, const uint16_t expected_id) const {
  if (len < sizeof(struct ip) + sizeof(icmphdr)) return false;

  const auto* ip_hdr = static_cast<const struct ip*>(data);
  const size_t ip_header_len = ip_hdr->ip_hl * 4;

  if (len < ip_header_len + sizeof(icmphdr)) return false;

  const auto* icmp_hdr = reinterpret_cast<const icmphdr*>(static_cast<const uint8_t*>(data) + ip_header_len);
  return ICMP_ECHOREPLY == icmp_hdr->type && expected_id == ntohs(icmp_hdr->un.echo.id);
}
#endif

// BSD IPv4 ICMP Handler
#ifndef __linux__
std::vector<uint8_t> bsd_ipv4_icmp_handler::build_echo_request(const uint16_t identifier, const uint16_t sequence) const {
  icmp icmp_pkt{};
  icmp_pkt.icmp_type = ICMP_ECHO;
  icmp_pkt.icmp_code = 0;
  icmp_pkt.icmp_hun.ih_idseq.icd_id = htons(identifier);
  icmp_pkt.icmp_hun.ih_idseq.icd_seq = htons(sequence);
  icmp_pkt.icmp_cksum = 0;
  icmp_pkt.icmp_cksum = calculate_icmp_checksum(&icmp_pkt, sizeof(icmp_pkt));

  std::vector<uint8_t> packet(sizeof(icmp_pkt));
  memcpy(packet.data(), &icmp_pkt, sizeof(icmp_pkt));
  return packet;
}

bool bsd_ipv4_icmp_handler::is_echo_reply_unprivileged(const void* data, const size_t len) const {
  if (len < sizeof(icmp)) return false;
  const auto* icmp_hdr = static_cast<const icmp*>(data);
  return ICMP_ECHOREPLY == icmp_hdr->icmp_type;
}

bool bsd_ipv4_icmp_handler::is_echo_reply_raw(const void* data, const size_t len, const uint16_t expected_id) const {
  if (len < sizeof(struct ip) + sizeof(icmp)) return false;

  const auto* ip_hdr = static_cast<const struct ip*>(data);
  const size_t ip_header_len = ip_hdr->ip_hl * 4;

  if (len < ip_header_len + sizeof(icmp)) return false;

  const auto* icmp_hdr = reinterpret_cast<const icmp*>(static_cast<const uint8_t*>(data) + ip_header_len);
  return ICMP_ECHOREPLY == icmp_hdr->icmp_type && expected_id == ntohs(icmp_hdr->icmp_hun.ih_idseq.icd_id);
}
#endif

// IPv6 ICMP Handler (same across platforms)
std::vector<uint8_t> ipv6_icmp_handler::build_echo_request(const uint16_t identifier, const uint16_t sequence) const {
  icmp6_hdr icmp6{};
  icmp6.icmp6_type = ICMP6_ECHO_REQUEST;
  icmp6.icmp6_code = 0;
  icmp6.icmp6_id = htons(identifier);
  icmp6.icmp6_seq = htons(sequence);

  std::vector<uint8_t> packet(sizeof(icmp6));
  memcpy(packet.data(), &icmp6, sizeof(icmp6));
  return packet;
}

bool ipv6_icmp_handler::is_echo_reply_unprivileged(const void* data, const size_t len) const {
  if (len < sizeof(icmp6_hdr)) return false;
  const auto* icmp6 = static_cast<const icmp6_hdr*>(data);
  return ICMP6_ECHO_REPLY == icmp6->icmp6_type;
}

bool ipv6_icmp_handler::is_echo_reply_raw(const void* data, const size_t len, const uint16_t expected_id) const {
  if (len < sizeof(icmp6_hdr)) return false;
  const auto* icmp6 = static_cast<const icmp6_hdr*>(data);
  return ICMP6_ECHO_REPLY == icmp6->icmp6_type && expected_id == ntohs(icmp6->icmp6_id);
}

// Factory Implementation
std::unique_ptr<icmp_packet_handler> icmp_handler_factory::create(const socket_family family) {
  if (socket_family::ipv6 == family) {
    return std::make_unique<ipv6_icmp_handler>();
  } else {
#ifdef __linux__
    return std::make_unique<linux_ipv4_icmp_handler>();
#else
    return std::make_unique<bsd_ipv4_icmp_handler>();
#endif
  }
}

test_result ping_tester_base::create_error_result(const std::string& error_msg, const long duration) {
  return test_result{false, duration, std::chrono::system_clock::now(), error_msg};
}

test_result ping_tester_base::create_success_result(const long duration) { return test_result{true, duration, std::chrono::system_clock::now(), std::nullopt}; }

// System Ping Tester Implementation
test_result system_ping_tester::ping_host(const std::string& host, const int timeout_ms) {
  const auto start_time = std::chrono::steady_clock::now();

  try {
    const std::string command = build_ping_command(host, timeout_ms);

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
      return create_error_result("Failed to execute ping command");
    }

    // Read ping output
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }

    const int result_code = pclose(pipe);

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (0 == result_code && parse_ping_output(output)) {
      return create_success_result(duration);
    } else {
      return create_error_result("Ping failed or host unreachable", duration);
    }

  } catch (const std::exception& e) {
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    return create_error_result(e.what(), duration);
  }
}

std::string system_ping_tester::build_ping_command(const std::string& host, const int timeout_ms) {
  std::ostringstream cmd;

  // Use ping for IPv4, ping6 for IPv6 (if needed)
  cmd << "ping -c 1 -W " << (timeout_ms / 1000 + 1) << " ";

  // Add host (with basic validation to prevent injection)
  if (std::string::npos != host.find_first_of(";&|`$(){}[]<>")) {
    throw std::invalid_argument("Invalid characters in hostname");
  }

  cmd << "'" << host << "' 2>/dev/null";

  return cmd.str();
}

bool system_ping_tester::parse_ping_output(const std::string& output) {
  // Look for patterns that indicate successful ping
  const std::regex success_patterns[] = {
      std::regex(R"(\d+ bytes from)"),                         // Standard ping success
      std::regex(R"(\d+ packets transmitted, \d+ received)"),  // Summary line
      std::regex(R"(time=\d+\.?\d*ms)")                        // Time measurement
  };

  for (const auto& pattern : success_patterns) {
    if (std::regex_search(output, pattern)) {
      return true;
    }
  }

  return false;
}

// ICMP Ping Tester Implementation
icmp_ping_tester::icmp_ping_tester() : icmp_socket_(-1), socket_initialized_(false) {
  socket_initialized_ = initialize_socket();
  if (!socket_initialized_) {
    spdlog::warn("Failed to initialize ICMP socket. ICMP ping tests will fail.");
  }
}

icmp_ping_tester::~icmp_ping_tester() { cleanup_socket(); }

test_result icmp_ping_tester::ping_host(const std::string& host, const int timeout_ms) {
  const auto start_time = std::chrono::steady_clock::now();

  if (!socket_initialized_) {
    return create_error_result("ICMP socket not initialized");
  }

  try {
    // Resolve hostname
    addrinfo hints{}, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4 for now
    hints.ai_socktype = SOCK_RAW;

    if (const int status = getaddrinfo(host.c_str(), nullptr, &hints, &result); 0 != status) {
      return create_error_result("DNS resolution failed");
    }

    bool ping_success = false;

    // Try pinging each resolved address
    for (const addrinfo* rp = result; rp != nullptr && !ping_success; rp = rp->ai_next) {
      if (send_icmp_packet(icmp_socket_, rp->ai_addr, rp->ai_addrlen)) {
        ping_success = wait_for_reply(icmp_socket_, timeout_ms);
      }
    }

    freeaddrinfo(result);

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (ping_success) {
      return create_success_result(duration);
    } else {
      return create_error_result("ICMP ping failed", duration);
    }

  } catch (const std::exception& e) {
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    return create_error_result(e.what(), duration);
  }
}

bool icmp_ping_tester::initialize_socket() {
  // Try to create an unprivileged ICMP socket
  icmp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
  if (icmp_socket_ < 0) {
    spdlog::debug("Failed to create unprivileged ICMP socket: {}", strerror(errno));
    return false;
  }

  return true;
}

bool icmp_ping_tester::send_icmp_packet(const int socket, const sockaddr* dest_addr, const socklen_t addr_len) {
  // For unprivileged ICMP, we send a minimal packet
  constexpr char packet[8] = {0};  // Minimal ICMP packet

  const ssize_t sent = sendto(socket, packet, sizeof(packet), 0, dest_addr, addr_len);
  return sent > 0;
}

bool icmp_ping_tester::wait_for_reply(const int socket, const int timeout_ms) {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(socket, &read_fds);

  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  if (const int result = select(socket + 1, &read_fds, nullptr, nullptr, &tv); result > 0 && FD_ISSET(socket, &read_fds)) {
    char buffer[1024];
    const ssize_t received = recv(socket, buffer, sizeof(buffer), 0);

    if (received > 0) {
      // For unprivileged ICMP sockets (SOCK_DGRAM), the kernel strips IP header
      // Use handler to check for echo reply (IPv4 for now)
      auto handler = icmp_handler_factory::create(icmp_handler_factory::socket_family::ipv4);
      return handler->is_echo_reply_unprivileged(buffer, received);
    }
  }

  return false;  // Timeout or error
}

void icmp_ping_tester::cleanup_socket() {
  if (icmp_socket_ >= 0) {
    close(icmp_socket_);
    icmp_socket_ = -1;
  }
  socket_initialized_ = false;
}

// Raw Socket Ping Tester Implementation
raw_socket_ping_tester::raw_socket_ping_tester() {
  // Initialize both IPv4 and IPv6 contexts
  ipv4_ctx_.family = socket_family::ipv4;
  ipv6_ctx_.family = socket_family::ipv6;

  // Generate random identifier
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint16_t> dis(1, UINT16_MAX);
  ipv4_ctx_.identifier = dis(gen);
  ipv6_ctx_.identifier = dis(gen);

  // Initialize sockets (don't fail constructor if they don't work)
  initialize_socket(socket_family::ipv4, ipv4_ctx_);
  initialize_socket(socket_family::ipv6, ipv6_ctx_);

  if (!ipv4_ctx_.socket_initialized && !ipv6_ctx_.socket_initialized) {
    spdlog::warn("Failed to initialize both IPv4 and IPv6 raw sockets. Raw socket ping tests will fail.");
  }
}

raw_socket_ping_tester::~raw_socket_ping_tester() {
  cleanup_socket(ipv4_ctx_);
  cleanup_socket(ipv6_ctx_);
}

test_result raw_socket_ping_tester::ping_host(const std::string& host, const int timeout_ms) {
  const auto start_time = std::chrono::steady_clock::now();

  try {
    // Determine address family and resolve hostname
    const socket_family family = determine_address_family(host);
    ping_context& ctx = (family == socket_family::ipv4) ? ipv4_ctx_ : ipv6_ctx_;

    if (!ctx.socket_initialized) {
      return create_error_result("Raw socket not initialized for this address family");
    }

    sockaddr_storage addr{};
    socklen_t addr_len = 0;

    if (!resolve_hostname(host, family, addr, addr_len)) {
      return create_error_result("DNS resolution failed");
    }

    bool ping_success = false;
    if (send_icmp_packet(ctx, reinterpret_cast<sockaddr*>(&addr), addr_len)) {
      ping_success = wait_for_reply(ctx, timeout_ms);
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (ping_success) {
      ctx.sequence++;
      return create_success_result(duration);
    } else {
      return create_error_result("Raw socket ping failed", duration);
    }

  } catch (const std::exception& e) {
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    return create_error_result(e.what(), duration);
  }
}

bool raw_socket_ping_tester::initialize_socket(const socket_family family, ping_context& ctx) {
  const int domain = (family == socket_family::ipv4) ? AF_INET : AF_INET6;
  const int protocol = (family == socket_family::ipv4) ? static_cast<int>(IPPROTO_ICMP) : static_cast<int>(IPPROTO_ICMPV6);

  ctx.socket_fd = socket(domain, SOCK_RAW, protocol);
  if (ctx.socket_fd < 0) {
    spdlog::debug("Failed to create raw {} socket: {}", (family == socket_family::ipv4) ? "IPv4" : "IPv6", strerror(errno));
    return false;
  }

  ctx.socket_initialized = true;
  return true;
}

bool raw_socket_ping_tester::send_icmp_packet(const ping_context& ctx, const sockaddr* dest_addr, const socklen_t addr_len) {
  // Use handler to build ICMP packet
  const auto family = (ctx.family == socket_family::ipv4) ?
    icmp_handler_factory::socket_family::ipv4 :
    icmp_handler_factory::socket_family::ipv6;

  auto handler = icmp_handler_factory::create(family);
  auto packet = handler->build_echo_request(ctx.identifier, ctx.sequence);

  const ssize_t sent = sendto(ctx.socket_fd, packet.data(), packet.size(), 0, dest_addr, addr_len);
  return sent > 0;
}

bool raw_socket_ping_tester::wait_for_reply(const ping_context& ctx, const int timeout_ms) {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(ctx.socket_fd, &read_fds);

  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  if (const int result = select(ctx.socket_fd + 1, &read_fds, nullptr, nullptr, &tv); result > 0 && FD_ISSET(ctx.socket_fd, &read_fds)) {
    char buffer[1024];
    const ssize_t received = recv(ctx.socket_fd, buffer, sizeof(buffer), 0);

    if (received > 0) {
      // Use handler to check for echo reply
      const auto family = (ctx.family == socket_family::ipv4) ?
        icmp_handler_factory::socket_family::ipv4 :
        icmp_handler_factory::socket_family::ipv6;

      auto handler = icmp_handler_factory::create(family);
      return handler->is_echo_reply_raw(buffer, received, ctx.identifier);
    }
  }

  return false;  // Timeout or error
}

void raw_socket_ping_tester::cleanup_socket(ping_context& ctx) {
  if (ctx.socket_fd >= 0) {
    close(ctx.socket_fd);
    ctx.socket_fd = -1;
  }
  ctx.socket_initialized = false;
}

raw_socket_ping_tester::socket_family raw_socket_ping_tester::determine_address_family(const std::string& host) {
  addrinfo hints{}, *result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;  // Allow both IPv4 and IPv6
  hints.ai_socktype = SOCK_RAW;

  if (const int status = getaddrinfo(host.c_str(), nullptr, &hints, &result); 0 != status) {
    // Default to IPv4 if resolution fails
    return socket_family::ipv4;
  }

  auto family = socket_family::ipv4;  // Default
  if (AF_INET6 == result->ai_family) {
    family = socket_family::ipv6;
  }

  freeaddrinfo(result);
  return family;
}

bool raw_socket_ping_tester::resolve_hostname(const std::string& host, const socket_family family, sockaddr_storage& addr, socklen_t& addr_len) {
  addrinfo hints{}, *result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (socket_family::ipv4 == family) ? AF_INET : AF_INET6;
  hints.ai_socktype = SOCK_RAW;

  if (const int status = getaddrinfo(host.c_str(), nullptr, &hints, &result); 0 != status) {
    return false;
  }

  memcpy(&addr, result->ai_addr, result->ai_addrlen);
  addr_len = result->ai_addrlen;

  freeaddrinfo(result);
  return true;
}

// Auto-fallback Ping Tester Implementation
auto_fallback_ping_tester::auto_fallback_ping_tester() {
  // Create implementations in order of preference:
  // 1. Unprivileged ICMP (most secure, modern)
  // 2. Raw socket (requires capabilities but works reliably)
  // 3. System ping (always works, but external dependency)

  implementations_.push_back(std::make_unique<icmp_ping_tester>());
  implementations_.push_back(std::make_unique<raw_socket_ping_tester>());
  implementations_.push_back(std::make_unique<system_ping_tester>());
}

test_result auto_fallback_ping_tester::ping_host(const std::string& host, const int timeout_ms) {
  test_result last_result = create_error_result("No ping implementation available");

  for (const auto& impl : implementations_) {
    test_result result = impl->ping_host(host, timeout_ms);

    if (result.is_success()) {
      current_implementation_ = impl->get_implementation_type();
      return result;
    }

    // Keep the last error for reporting
    last_result = result;
  }

  // All implementations failed
  current_implementation_ = ping_implementation::system_ping;  // Default for reporting
  return last_result;
}

// Factory Implementation
std::unique_ptr<ping_tester_base> ping_tester_factory::create(const ping_implementation impl) {
  switch (impl) {
    case ping_implementation::system_ping:
      return std::make_unique<system_ping_tester>();
    case ping_implementation::unprivileged_icmp:
      return std::make_unique<icmp_ping_tester>();
    case ping_implementation::raw_socket:
      return std::make_unique<raw_socket_ping_tester>();
    default:
      throw std::invalid_argument("Unsupported ping implementation");
  }
}

std::unique_ptr<ping_tester_base> ping_tester_factory::create_auto_fallback() { return std::make_unique<auto_fallback_ping_tester>(); }