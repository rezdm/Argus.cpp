#pragma once

#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "push_config.h"
#include "push_subscription.h"

class push_notification_manager {
 public:
  explicit push_notification_manager(const push_notification_config& config);
  ~push_notification_manager();

  // Subscription management
  bool add_subscription(const push_subscription& subscription);
  bool remove_subscription(const std::string& endpoint);
  [[nodiscard]] size_t get_subscription_count() const;
  void clear_subscriptions();

  // Notification sending
  bool send_notification(const std::string& title, const std::string& body, const std::string& icon = "", const nlohmann::json& data = nlohmann::json::object());
  bool send_notification_to(const std::string& endpoint, const std::string& title, const std::string& body, const std::string& icon = "", const nlohmann::json& data = nlohmann::json::object());
  bool send_notification_for_test(const std::string& test_id, const std::string& title, const std::string& body, const std::string& icon = "", const nlohmann::json& data = nlohmann::json::object());

  // Suppression management
  bool add_suppression(const std::string& test_id, const std::string& until_timestamp);
  bool remove_suppression(const std::string& test_id);
  [[nodiscard]] bool is_suppressed(const std::string& test_id) const;
  [[nodiscard]] nlohmann::json get_all_suppressions() const;

  // Persistence
  bool load_subscriptions(const std::string& filepath);
  bool save_subscriptions(const std::string& filepath) const;
  bool load_suppressions(const std::string& filepath);
  bool save_suppressions(const std::string& filepath) const;

  [[nodiscard]] bool is_enabled() const { return config_.enabled; }

 private:
  push_notification_config config_;
  std::vector<push_subscription> subscriptions_;
  mutable std::mutex subscriptions_mutex_;

  // Suppressions: map of test_id -> until_timestamp (ISO 8601 format)
  std::map<std::string, std::string> suppressions_;
  mutable std::mutex suppressions_mutex_;

  // Send push message via Web Push protocol
  bool send_web_push(const push_subscription& subscription, const nlohmann::json& payload);

  // Helper: Build JWT for VAPID authentication
  [[nodiscard]] std::string build_vapid_jwt(const std::string& audience) const;

  // Helper: Extract origin from endpoint URL
  [[nodiscard]] static std::string extract_origin(const std::string& endpoint);

  // Helper: Check if timestamp is in the past
  [[nodiscard]] static bool is_timestamp_past(const std::string& timestamp);
};
