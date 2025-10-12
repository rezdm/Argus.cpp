#pragma once

#include <nlohmann/json.hpp>
#include <string>

// Push subscription structure matching Web Push API format
struct push_subscription {
  std::string endpoint;
  std::string p256dh;  // Base64url-encoded client public key (ECDH)
  std::string auth;    // Base64url-encoded authentication secret

  [[nodiscard]] nlohmann::json to_json() const {
    nlohmann::json j;
    j["endpoint"] = endpoint;
    j["keys"]["p256dh"] = p256dh;
    j["keys"]["auth"] = auth;
    return j;
  }

  static push_subscription from_json(const nlohmann::json& j) {
    push_subscription sub;
    sub.endpoint = j.at("endpoint").get<std::string>();
    sub.p256dh = j.at("keys").at("p256dh").get<std::string>();
    sub.auth = j.at("keys").at("auth").get<std::string>();
    return sub;
  }
};
