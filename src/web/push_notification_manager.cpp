#include "push_notification_manager.h"
#include "webpush_encryption.h"
#include "vapid_jwt.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <httplib.h>

using argus::webpush_encryption;
using argus::vapid_jwt;

push_notification_manager::push_notification_manager(const push_notification_config& config) : config_(config) {
  if (config_.enabled) {
    spdlog::info("Push notification manager initialized (VAPID subject: {})", config_.vapid_subject);

    if (!config_.is_valid()) {
      spdlog::error("Invalid push notification configuration: {}", config_.get_validation_error());
      config_.enabled = false;
    }
  } else {
    spdlog::info("Push notifications are disabled");
  }
}

push_notification_manager::~push_notification_manager() {
  spdlog::debug("Push notification manager destroyed ({} subscriptions)", subscriptions_.size());
}

bool push_notification_manager::add_subscription(const push_subscription& subscription) {
  if (!config_.enabled) {
    return false;
  }

  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  // Check if subscription already exists
  const auto it = std::ranges::find_if(subscriptions_, [&subscription](const push_subscription& sub) { return sub.endpoint == subscription.endpoint; });

  if (subscriptions_.end() != it) {
    spdlog::debug("Subscription already exists, updating: {}", subscription.endpoint);
    *it = subscription;
  } else {
    spdlog::info("Adding new push subscription: {}", subscription.endpoint.substr(0, 50) + "...");
    subscriptions_.push_back(subscription);
  }

  return true;
}

bool push_notification_manager::remove_subscription(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  const auto it = std::ranges::find_if(subscriptions_, [&endpoint](const push_subscription& sub) { return sub.endpoint == endpoint; });

  if (subscriptions_.end() != it) {
    spdlog::info("Removing push subscription: {}", endpoint.substr(0, 50) + "...");
    subscriptions_.erase(it);
    return true;
  }

  return false;
}

size_t push_notification_manager::get_subscription_count() const {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);
  return subscriptions_.size();
}

void push_notification_manager::clear_subscriptions() {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);
  spdlog::info("Clearing all push subscriptions ({} total)", subscriptions_.size());
  subscriptions_.clear();
}

bool push_notification_manager::send_notification(const std::string& title, const std::string& body, const std::string& icon, const nlohmann::json& data) {
  if (!config_.enabled) {
    spdlog::debug("Push notifications disabled, skipping notification: {}", title);
    return false;
  }

  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  if (subscriptions_.empty()) {
    spdlog::debug("No push subscriptions available");
    return false;
  }

  spdlog::info("Sending push notification to {} subscribers: {}", subscriptions_.size(), title);

  bool any_success = false;
  std::vector<std::string> failed_endpoints;

  for (const auto& subscription : subscriptions_) {
    nlohmann::json payload = {{"title", title}, {"body", body}, {"icon", icon.empty() ? "/icons/icon-192x192.png" : icon}, {"data", data}, {"tag", "argus-notification"}, {"requireInteraction", true}};

    if (send_web_push(subscription, payload)) {
      any_success = true;
    } else {
      failed_endpoints.push_back(subscription.endpoint);
    }
  }

  // Remove failed subscriptions (they might be invalid/expired)
  if (!failed_endpoints.empty()) {
    spdlog::warn("Removing {} failed/expired subscriptions", failed_endpoints.size());
    for (const auto& endpoint : failed_endpoints) {
      std::erase_if(subscriptions_, [&endpoint](const push_subscription& sub) { return sub.endpoint == endpoint; });
    }
  }

  return any_success;
}

bool push_notification_manager::send_notification_to(const std::string& endpoint, const std::string& title, const std::string& body, const std::string& icon, const nlohmann::json& data) {
  if (!config_.enabled) {
    return false;
  }

  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  const auto it = std::ranges::find_if(subscriptions_, [&endpoint](const push_subscription& sub) { return sub.endpoint == endpoint; });

  if (subscriptions_.end() == it) {
    spdlog::warn("Subscription not found: {}", endpoint);
    return false;
  }

  const nlohmann::json payload = {{"title", title}, {"body", body}, {"icon", icon.empty() ? "/icons/icon-192x192.png" : icon}, {"data", data}, {"tag", "argus-notification"}, {"requireInteraction", true}};

  return send_web_push(*it, payload);
}

bool push_notification_manager::send_web_push(const push_subscription& subscription, const nlohmann::json& payload) {
  try {
    spdlog::debug("Sending Web Push to: {}", subscription.endpoint.substr(0, 80));

    // Step 1: Encrypt the payload using RFC 8291 (aes128gcm)
    const std::string payload_str = payload.dump();
    const auto encrypted = webpush_encryption::encrypt(payload_str, subscription);
    const auto body = webpush_encryption::build_request_body(encrypted);

    spdlog::debug("Encrypted payload: {} bytes", body.size());

    // Step 2: Extract origin for VAPID audience
    std::string origin = extract_origin(subscription.endpoint);
    spdlog::debug("Push service origin: {}", origin);

    // Step 3: Build VAPID JWT
    const std::string jwt = build_vapid_jwt(origin);
    if (jwt.empty()) {
      spdlog::error("Failed to build VAPID JWT");
      return false;
    }

    // Step 4: Parse endpoint URL
    const size_t protocol_end = subscription.endpoint.find("://");
    if (protocol_end == std::string::npos) {
      spdlog::error("Invalid endpoint URL: {}", subscription.endpoint);
      return false;
    }

    bool is_https = subscription.endpoint.substr(0, protocol_end) == "https";
    const size_t host_start = protocol_end + 3;
    const size_t path_start = subscription.endpoint.find('/', host_start);

    std::string host;
    std::string path;
    int port = is_https ? 443 : 80;

    if (path_start == std::string::npos) {
      host = subscription.endpoint.substr(host_start);
      path = "/";
    } else {
      host = subscription.endpoint.substr(host_start, path_start - host_start);
      path = subscription.endpoint.substr(path_start);
    }

    // Check for port in host
    const size_t port_sep = host.find(':');
    if (port_sep != std::string::npos) {
      port = std::stoi(host.substr(port_sep + 1));
      host = host.substr(0, port_sep);
    }

    spdlog::debug("Connecting to {}:{}{} (https={})", host, port, path, is_https);
    spdlog::debug("JWT (first 50 chars): {}", jwt.substr(0, std::min(50UL, jwt.size())));
    spdlog::debug("Public key: {}", config_.vapid_public_key);
    spdlog::debug("Encrypted body size: {} bytes", body.size());

    // Step 5: Send HTTPS POST request
    httplib::SSLClient client(host, port);
    client.set_follow_location(true);
    client.set_connection_timeout(10);
    client.set_read_timeout(10);
    client.set_write_timeout(10);

    const httplib::Headers headers = {
      {"Content-Type", "application/octet-stream"},
      {"Content-Encoding", "aes128gcm"},
      {"TTL", "86400"},  // 24 hours
      {"Authorization", "vapid t=" + jwt + ", k=" + config_.vapid_public_key}
    };

    auto res = client.Post(path, headers, reinterpret_cast<const char*>(body.data()), body.size(), "application/octet-stream");

    if (!res) {
      spdlog::error("HTTP request failed: {}", httplib::to_string(res.error()));
      return false;
    }

    if (res->status >= 200 && res->status < 300) {
      spdlog::info("✓ Push notification sent successfully (status: {})", res->status);
      return true;
    } else if (res->status == 410 || res->status == 404) {
      spdlog::warn("Push subscription expired or invalid (status: {})", res->status);
      if (!res->body.empty()) {
        spdlog::debug("Response body: {}", res->body);
      }
      return false;  // Will trigger subscription removal
    } else {
      spdlog::error("Push service returned error: {} - {}", res->status, res->body);
      return false;
    }

  } catch (const std::exception& e) {
    spdlog::error("Exception sending Web Push: {}", e.what());
    return false;
  }
}

std::string push_notification_manager::build_vapid_jwt(const std::string& audience) const {
  try {
    return vapid_jwt::build(
      audience,
      config_.vapid_subject,
      config_.vapid_private_key,
      43200  // 12 hours
    );
  } catch (const std::exception& e) {
    spdlog::error("Failed to build VAPID JWT: {}", e.what());
    return "";
  }
}

std::string push_notification_manager::extract_origin(const std::string& endpoint) {
  // Extract origin (protocol + host) from endpoint URL
  const size_t protocol_end = endpoint.find("://");
  if (std::string::npos == protocol_end) {
    return "";
  }

  const size_t host_start = protocol_end + 3;
  const size_t path_start = endpoint.find('/', host_start);

  if (std::string::npos == path_start) {
    return endpoint;  // No path, entire URL is origin
  }

  return endpoint.substr(0, path_start);
}

bool push_notification_manager::load_subscriptions(const std::string& filepath) {
  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      spdlog::debug("No subscription file found: {}", filepath);
      return false;
    }

    nlohmann::json j;
    file >> j;

    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_.clear();

    for (const auto& sub_json : j["subscriptions"]) {
      subscriptions_.push_back(push_subscription::from_json(sub_json));
    }

    spdlog::info("Loaded {} push subscriptions from {}", subscriptions_.size(), filepath);
    return true;

  } catch (const std::exception& e) {
    spdlog::error("Failed to load subscriptions: {}", e.what());
    return false;
  }
}

bool push_notification_manager::save_subscriptions(const std::string& filepath) const {
  try {
    nlohmann::json j;
    j["subscriptions"] = nlohmann::json::array();

    {
      std::lock_guard<std::mutex> lock(subscriptions_mutex_);
      for (const auto& sub : subscriptions_) {
        j["subscriptions"].push_back(sub.to_json());
      }
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
      spdlog::error("Failed to open file for writing: {}", filepath);
      return false;
    }

    file << j.dump(2);
    spdlog::debug("Saved {} push subscriptions to {}", j["subscriptions"].size(), filepath);
    return true;

  } catch (const std::exception& e) {
    spdlog::error("Failed to save subscriptions: {}", e.what());
    return false;
  }
}
