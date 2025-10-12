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

  // Persistence
  bool load_subscriptions(const std::string& filepath);
  bool save_subscriptions(const std::string& filepath) const;

  [[nodiscard]] bool is_enabled() const { return config_.enabled; }

 private:
  push_notification_config config_;
  std::vector<push_subscription> subscriptions_;
  mutable std::mutex subscriptions_mutex_;

  // Send push message via Web Push protocol
  bool send_web_push(const push_subscription& subscription, const nlohmann::json& payload);

  // Helper: Build JWT for VAPID authentication
  [[nodiscard]] std::string build_vapid_jwt(const std::string& audience) const;

  // Helper: Extract origin from endpoint URL
  [[nodiscard]] static std::string extract_origin(const std::string& endpoint);
};
