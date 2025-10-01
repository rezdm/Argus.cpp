#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../core/types.h"

// Platform-specific socket type includes
#ifdef __FreeBSD__
#define _DEFAULT_SOURCE
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
// Ensure socklen_t is available on FreeBSD
#ifndef socklen_t
typedef __socklen_t socklen_t;
#endif
#elif defined(__linux__)
// Linux typically has socklen_t available
#else
// Other platforms
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

enum class ping_implementation { system_ping, unprivileged_icmp, raw_socket };

// ICMP Packet Handler Strategy - abstracts platform-specific ICMP operations
class icmp_packet_handler {
 public:
  virtual ~icmp_packet_handler() = default;

  // Build an ICMP echo request packet
  virtual std::vector<uint8_t> build_echo_request(uint16_t identifier, uint16_t sequence) const = 0;

  // Check if received data is an ICMP echo reply for unprivileged sockets (SOCK_DGRAM)
  virtual bool is_echo_reply_unprivileged(const void* data, size_t len) const = 0;

  // Check if received data is an ICMP echo reply for raw sockets (SOCK_RAW)
  // Returns true if it's a reply with matching identifier
  virtual bool is_echo_reply_raw(const void* data, size_t len, uint16_t expected_id) const = 0;
};

// Linux IPv4 ICMP handler
class linux_ipv4_icmp_handler final : public icmp_packet_handler {
 public:
  std::vector<uint8_t> build_echo_request(uint16_t identifier, uint16_t sequence) const override;
  bool is_echo_reply_unprivileged(const void* data, size_t len) const override;
  bool is_echo_reply_raw(const void* data, size_t len, uint16_t expected_id) const override;
};

// BSD-like (FreeBSD, Solaris) IPv4 ICMP handler
class bsd_ipv4_icmp_handler final : public icmp_packet_handler {
 public:
  std::vector<uint8_t> build_echo_request(uint16_t identifier, uint16_t sequence) const override;
  bool is_echo_reply_unprivileged(const void* data, size_t len) const override;
  bool is_echo_reply_raw(const void* data, size_t len, uint16_t expected_id) const override;
};

// IPv6 ICMP handler (same across platforms)
class ipv6_icmp_handler final : public icmp_packet_handler {
 public:
  std::vector<uint8_t> build_echo_request(uint16_t identifier, uint16_t sequence) const override;
  bool is_echo_reply_unprivileged(const void* data, size_t len) const override;
  bool is_echo_reply_raw(const void* data, size_t len, uint16_t expected_id) const override;
};

// Factory for creating ICMP handlers
class icmp_handler_factory {
 public:
  enum class socket_family { ipv4, ipv6 };
  static std::unique_ptr<icmp_packet_handler> create(socket_family family);
};

class ping_tester_base {
 public:
  virtual ~ping_tester_base() = default;
  virtual test_result ping_host(const std::string& host, int timeout_ms) = 0;
  [[nodiscard]] virtual ping_implementation get_implementation_type() const = 0;

 protected:
  static test_result create_error_result(const std::string& error_msg, long duration = 0);
  static test_result create_success_result(long duration);
};

class system_ping_tester final : public ping_tester_base {
 public:
  test_result ping_host(const std::string& host, int timeout_ms) override;
  [[nodiscard]] ping_implementation get_implementation_type() const override { return ping_implementation::system_ping; }

 private:
  static std::string build_ping_command(const std::string& host, int timeout_ms);

  static bool parse_ping_output(const std::string& output);
};

class icmp_ping_tester final : public ping_tester_base {
 public:
  icmp_ping_tester();
  ~icmp_ping_tester() override;

  test_result ping_host(const std::string& host, int timeout_ms) override;
  [[nodiscard]] ping_implementation get_implementation_type() const override { return ping_implementation::unprivileged_icmp; }

 private:
  bool initialize_socket();

  static bool send_icmp_packet(int socket, const struct sockaddr* dest_addr, socklen_t addr_len);

  static bool wait_for_reply(int socket, int timeout_ms);
  void cleanup_socket();

  int icmp_socket_;
  bool socket_initialized_;
};

class raw_socket_ping_tester final : public ping_tester_base {
 public:
  raw_socket_ping_tester();
  ~raw_socket_ping_tester() override;

  test_result ping_host(const std::string& host, int timeout_ms) override;
  [[nodiscard]] ping_implementation get_implementation_type() const override { return ping_implementation::raw_socket; }

 private:
  enum class socket_family { ipv4, ipv6 };

  struct ping_context {
    int socket_fd = -1;
    socket_family family;
    uint16_t sequence = 1;
    uint16_t identifier;
    bool socket_initialized = false;
  };

  static bool initialize_socket(socket_family family, ping_context& ctx);

  static bool send_icmp_packet(const ping_context& ctx, const sockaddr* dest_addr, socklen_t addr_len);

  static bool wait_for_reply(const ping_context& ctx, int timeout_ms);

  static void cleanup_socket(ping_context& ctx);

  static socket_family determine_address_family(const std::string& host);
  static bool resolve_hostname(const std::string& host, socket_family family, struct sockaddr_storage& addr, socklen_t& addr_len);

  ping_context ipv4_ctx_;
  ping_context ipv6_ctx_;
};

class auto_fallback_ping_tester final : public ping_tester_base {
 public:
  auto_fallback_ping_tester();
  ~auto_fallback_ping_tester() override = default;

  test_result ping_host(const std::string& host, int timeout_ms) override;
  [[nodiscard]] ping_implementation get_implementation_type() const override { return current_implementation_; }

 private:
  std::vector<std::unique_ptr<ping_tester_base>> implementations_;
  ping_implementation current_implementation_ = ping_implementation::system_ping;
};

class ping_tester_factory {
 public:
  static std::unique_ptr<ping_tester_base> create_auto_fallback();

 private:
  static std::unique_ptr<ping_tester_base> create(ping_implementation impl);
};