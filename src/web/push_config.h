#pragma once

#include <string>

struct push_notification_config {
  bool enabled = false;
  std::string vapid_subject;      // e.g., "mailto:your-email@example.com"
  std::string vapid_public_key;   // Base64-encoded VAPID public key
  std::string vapid_private_key;  // Base64-encoded VAPID private key
  std::string subscriptions_file = "push_subscriptions.json";  // Path to subscriptions file

  [[nodiscard]] bool is_valid() const {
    if (!enabled) return true;  // Valid if disabled
    return !vapid_subject.empty() && !vapid_public_key.empty() && !vapid_private_key.empty();
  }

  [[nodiscard]] std::string get_validation_error() const {
    if (!enabled) return "";
    if (vapid_subject.empty()) return "VAPID subject cannot be empty when push notifications are enabled";
    if (vapid_public_key.empty()) return "VAPID public key cannot be empty when push notifications are enabled";
    if (vapid_private_key.empty()) return "VAPID private key cannot be empty when push notifications are enabled";
    return "";
  }
};
