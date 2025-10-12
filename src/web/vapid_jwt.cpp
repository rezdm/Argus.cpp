#include "vapid_jwt.h"
#include "crypto_utils.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <stdexcept>

namespace argus {

using json = nlohmann::json;
using namespace argus::crypto;

uint64_t vapid_jwt::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    return static_cast<uint64_t>(seconds.count());
}

std::string vapid_jwt::extract_audience(const std::string& endpoint) {
    // Extract protocol and host from URL
    // Example: https://fcm.googleapis.com/fcm/send/... -> https://fcm.googleapis.com

    size_t protocol_end = endpoint.find("://");
    if (protocol_end == std::string::npos) {
        throw std::runtime_error("Invalid endpoint URL: missing protocol");
    }

    size_t host_start = protocol_end + 3;
    size_t path_start = endpoint.find('/', host_start);

    if (path_start == std::string::npos) {
        // No path, entire string is origin
        return endpoint;
    }

    return endpoint.substr(0, path_start);
}

std::string vapid_jwt::build(
    const std::string& audience,
    const std::string& subject,
    const std::string& private_key_pem,
    uint32_t expiration_seconds
) {
    spdlog::debug("Building VAPID JWT: aud={}, sub={}, exp={}s", audience, subject, expiration_seconds);

    // Validate subject format (must be mailto: or https:)
    if (subject.substr(0, 7) != "mailto:" && subject.substr(0, 8) != "https://") {
        throw std::runtime_error("Invalid subject: must start with 'mailto:' or 'https://'");
    }

    // Step 1: Build JWT header
    json header = {
        {"typ", "JWT"},
        {"alg", "ES256"}
    };
    std::string header_json = header.dump();
    std::string header_b64 = base64url::encode(header_json);

    spdlog::debug("JWT header: {}", header_json);

    // Step 2: Build JWT payload
    uint64_t now = get_current_timestamp();
    uint64_t exp = now + expiration_seconds;

    json payload = {
        {"aud", audience},
        {"exp", exp},
        {"sub", subject}
    };
    std::string payload_json = payload.dump();
    std::string payload_b64 = base64url::encode(payload_json);

    spdlog::debug("JWT payload: {}", payload_json);

    // Step 3: Create signing input (header.payload)
    std::string signing_input = header_b64 + "." + payload_b64;

    // Step 4: Sign using ES256 (ECDSA with P-256 and SHA-256)
    std::vector<uint8_t> signing_input_bytes(signing_input.begin(), signing_input.end());
    std::vector<uint8_t> signature;

    if (!ecdsa::sign_es256(signing_input_bytes, private_key_pem, signature)) {
        throw std::runtime_error("Failed to sign JWT with ES256");
    }

    if (signature.size() != 64) {
        throw std::runtime_error("Invalid signature size: " + std::to_string(signature.size()));
    }

    // Step 5: Encode signature
    std::string signature_b64 = base64url::encode(signature);

    // Step 6: Build final JWT (header.payload.signature)
    std::string jwt = signing_input + "." + signature_b64;

    spdlog::debug("Generated VAPID JWT: {} bytes", jwt.size());

    return jwt;
}

} // namespace argus
