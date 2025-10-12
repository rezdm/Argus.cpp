#pragma once

#include <string>
#include <cstdint>

namespace argus {

/**
 * VAPID (Voluntary Application Server Identification) JWT builder
 *
 * Implements RFC 8292 for authenticating Web Push requests using ES256 (ECDSA with P-256 and SHA-256).
 * This allows push services to identify the application server sending push notifications.
 */
class vapid_jwt {
public:
    /**
     * Build a VAPID JWT token for authenticating push requests
     *
     * @param audience The audience claim (origin of the push service, e.g., "https://fcm.googleapis.com")
     * @param subject The subject claim (mailto: or https: URL identifying the app, e.g., "mailto:admin@example.com")
     * @param private_key_pem PEM-encoded ECDSA P-256 private key
     * @param expiration_seconds Token validity duration (default: 12 hours)
     * @return JWT token string in format: header.payload.signature
     * @throws std::runtime_error if signing fails
     */
    static std::string build(
        const std::string& audience,
        const std::string& subject,
        const std::string& private_key_pem,
        uint32_t expiration_seconds = 43200  // 12 hours default
    );

    /**
     * Extract the push service origin (audience) from an endpoint URL
     * Example: "https://fcm.googleapis.com/fcm/send/..." -> "https://fcm.googleapis.com"
     *
     * @param endpoint The push subscription endpoint URL
     * @return The origin (protocol + host)
     */
    static std::string extract_audience(const std::string& endpoint);

private:
    /**
     * Get current Unix timestamp in seconds
     */
    static uint64_t get_current_timestamp();
};

} // namespace argus
