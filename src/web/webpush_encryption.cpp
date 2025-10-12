#include "webpush_encryption.h"
#include "crypto_utils.h"
#include <openssl/rand.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstring>

namespace argus {

using namespace argus::crypto;

// Generate 16 random bytes for salt
std::vector<uint8_t> webpush_encryption::generate_salt() {
    std::vector<uint8_t> salt(16);
    if (RAND_bytes(salt.data(), 16) != 1) {
        throw std::runtime_error("Failed to generate random salt");
    }
    return salt;
}

// Create info string for HKDF according to RFC 8291
std::vector<uint8_t> webpush_encryption::create_info(
    const std::string& type,
    const std::vector<uint8_t>& client_public_key,
    const std::vector<uint8_t>& server_public_key
) {
    // Info format: "Content-Encoding: <type>\0" || "P-256" || client_key || server_key
    std::string prefix = "Content-Encoding: " + type;
    std::vector<uint8_t> info;

    // Add prefix with null terminator
    info.insert(info.end(), prefix.begin(), prefix.end());
    info.push_back(0x00);

    // Add "P-256" (without null terminator for context)
    const char* curve_name = "P-256";
    info.insert(info.end(), curve_name, curve_name + 5);
    info.push_back(0x00);

    // Add client public key length (2 bytes, big-endian)
    uint16_t client_key_len = static_cast<uint16_t>(client_public_key.size());
    info.push_back(static_cast<uint8_t>(client_key_len >> 8));
    info.push_back(static_cast<uint8_t>(client_key_len & 0xFF));

    // Add client public key
    info.insert(info.end(), client_public_key.begin(), client_public_key.end());

    // Add server public key length (2 bytes, big-endian)
    uint16_t server_key_len = static_cast<uint16_t>(server_public_key.size());
    info.push_back(static_cast<uint8_t>(server_key_len >> 8));
    info.push_back(static_cast<uint8_t>(server_key_len & 0xFF));

    // Add server public key
    info.insert(info.end(), server_public_key.begin(), server_public_key.end());

    // Debug: log first 50 bytes of info
    std::string info_hex;
    for (size_t i = 0; i < std::min(size_t(50), info.size()); i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02x ", info[i]);
        info_hex += buf;
    }
    spdlog::debug("Info '{}' hex (first 50): {}", type, info_hex);

    return info;
}

// Derive encryption key and nonce using HKDF-SHA256
void webpush_encryption::derive_keys(
    const std::vector<uint8_t>& shared_secret,
    const std::vector<uint8_t>& auth_secret,
    const std::vector<uint8_t>& salt,
    const std::vector<uint8_t>& client_public_key,
    const std::vector<uint8_t>& server_public_key,
    std::vector<uint8_t>& content_encryption_key,
    std::vector<uint8_t>& nonce
) {
    // RFC 8291 Section 3.3: Key derivation using combined HKDF
    // Step 1: Derive IKM from shared secret and auth secret
    std::string auth_info_str = "Content-Encoding: auth";
    std::vector<uint8_t> auth_info(auth_info_str.begin(), auth_info_str.end());
    auth_info.push_back(0x00);
    std::vector<uint8_t> ikm = hkdf::derive(shared_secret, auth_secret, auth_info, 32);

    // Step 2: Create key_info for deriving the encryption key
    auto key_info = create_info("aes128gcm", client_public_key, server_public_key);
    spdlog::debug("Key info size: {} bytes", key_info.size());

    // Step 3: Derive content encryption key (16 bytes for AES-128)
    content_encryption_key = hkdf::derive(ikm, salt, key_info, 16);

    // Step 4: Create nonce_info for deriving the nonce
    auto nonce_info = create_info("nonce", client_public_key, server_public_key);
    spdlog::debug("Nonce info size: {} bytes", nonce_info.size());

    // Step 5: Derive nonce (12 bytes for AES-GCM)
    nonce = hkdf::derive(ikm, salt, nonce_info, 12);
}

webpush_encryption::encrypted_payload webpush_encryption::encrypt(
    const std::string& plaintext,
    const push_subscription& subscription
) {
    spdlog::debug("Encrypting Web Push payload of {} bytes", plaintext.size());

    // Step 1: Decode the client's public key (p256dh) from base64url
    std::vector<uint8_t> client_public_key = base64url::decode(subscription.p256dh);
    if (client_public_key.size() != 65) {
        throw std::runtime_error("Invalid client public key size: " +
                                std::to_string(client_public_key.size()));
    }
    spdlog::debug("Client public key: {} bytes", client_public_key.size());

    // Step 2: Generate a new ephemeral ECDH key pair (server keys)
    std::vector<uint8_t> server_public_key;
    std::vector<uint8_t> server_private_key;
    if (!ecdh::generate_keypair(server_public_key, server_private_key)) {
        throw std::runtime_error("Failed to generate ECDH key pair");
    }
    spdlog::debug("Generated server key pair: public={} bytes, private={} bytes",
                 server_public_key.size(), server_private_key.size());

    // Step 3: Compute shared secret using ECDH
    std::vector<uint8_t> shared_secret;
    if (!ecdh::compute_shared_secret(server_private_key, client_public_key, shared_secret)) {
        throw std::runtime_error("Failed to compute ECDH shared secret");
    }
    spdlog::debug("Computed shared secret: {} bytes", shared_secret.size());

    // Step 4: Decode the auth secret from base64url
    std::vector<uint8_t> auth_secret = base64url::decode(subscription.auth);
    if (auth_secret.size() != 16) {
        throw std::runtime_error("Invalid auth secret size: " +
                                std::to_string(auth_secret.size()));
    }
    spdlog::debug("Auth secret: {} bytes", auth_secret.size());

    // Step 5: Generate random salt
    std::vector<uint8_t> salt = generate_salt();
    spdlog::debug("Generated salt: {} bytes", salt.size());

    // Step 6: Derive content encryption key and nonce
    std::vector<uint8_t> content_encryption_key;
    std::vector<uint8_t> nonce;
    derive_keys(shared_secret, auth_secret, salt, client_public_key, server_public_key,
                content_encryption_key, nonce);
    spdlog::debug("Derived CEK: {} bytes, nonce: {} bytes",
                 content_encryption_key.size(), nonce.size());

    // Step 7: Prepare plaintext with padding
    // RFC 8291 Format: plaintext || 0x02 || padding_bytes
    // The delimiter 0x02 marks the end of content, followed by optional padding
    std::vector<uint8_t> padded_plaintext(plaintext.begin(), plaintext.end());
    padded_plaintext.push_back(0x02);  // Delimiter marking end of content
    // No additional padding bytes needed (padding is optional after the delimiter)

    spdlog::debug("Padded plaintext: {} bytes (original: {} bytes)",
                 padded_plaintext.size(), plaintext.size());

    // Step 8: Encrypt using AES-128-GCM
    std::vector<uint8_t> ciphertext;
    if (!aes_gcm::encrypt(padded_plaintext, content_encryption_key, nonce, ciphertext)) {
        throw std::runtime_error("AES-GCM encryption failed");
    }
    spdlog::debug("Encrypted ciphertext: {} bytes (includes 16-byte auth tag)", ciphertext.size());

    encrypted_payload result;
    result.ciphertext = std::move(ciphertext);
    result.salt = std::move(salt);
    result.server_public_key = std::move(server_public_key);

    return result;
}

std::vector<uint8_t> webpush_encryption::build_request_body(const encrypted_payload& payload) {
    /*
     * Binary format for aes128gcm (RFC 8291):
     * - salt: 16 bytes
     * - rs (record size): 4 bytes (big-endian uint32, fixed at 4096)
     * - idlen (key ID length): 1 byte (value: 65 for uncompressed P-256 key)
     * - keyid (server public key): 65 bytes
     * - ciphertext: variable length (plaintext + padding + 16-byte auth tag)
     */

    std::vector<uint8_t> body;
    body.reserve(16 + 4 + 1 + 65 + payload.ciphertext.size());

    // Add salt (16 bytes)
    body.insert(body.end(), payload.salt.begin(), payload.salt.end());

    // Add record size (4 bytes, big-endian, value: 4096)
    uint32_t record_size = 4096;
    body.push_back(static_cast<uint8_t>((record_size >> 24) & 0xFF));
    body.push_back(static_cast<uint8_t>((record_size >> 16) & 0xFF));
    body.push_back(static_cast<uint8_t>((record_size >> 8) & 0xFF));
    body.push_back(static_cast<uint8_t>(record_size & 0xFF));

    // Add key ID length (1 byte, value: 65)
    body.push_back(65);

    // Add server public key (65 bytes)
    body.insert(body.end(), payload.server_public_key.begin(), payload.server_public_key.end());

    // Add ciphertext (already includes 16-byte authentication tag)
    body.insert(body.end(), payload.ciphertext.begin(), payload.ciphertext.end());

    spdlog::debug("Built request body: {} bytes total (salt=16, header=5, key=65, ciphertext={})",
                 body.size(), payload.ciphertext.size());

    // Log complete body as hex for debugging
    std::string hex_dump;
    for (size_t i = 0; i < body.size(); i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02x ", body[i]);
        hex_dump += buf;
        // Add newline every 32 bytes for readability
        if ((i + 1) % 32 == 0 && i + 1 < body.size()) {
            hex_dump += "\n";
        }
    }
    spdlog::debug("Body hex (complete {} bytes):\n{}", body.size(), hex_dump);

    return body;
}

} // namespace argus
