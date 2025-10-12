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

namespace {

std::vector<uint8_t> build_webpush_context(const std::vector<uint8_t>& client_public_key,
                                          const std::vector<uint8_t>& server_public_key) {
    std::vector<uint8_t> context;
    context.reserve(13 + client_public_key.size() + server_public_key.size());

    const std::string label = "WebPush: info";
    context.insert(context.end(), label.begin(), label.end());
    context.push_back(0x00);
    context.insert(context.end(), client_public_key.begin(), client_public_key.end());
    context.insert(context.end(), server_public_key.begin(), server_public_key.end());

    return context;
}

std::vector<uint8_t> build_content_encoding_label(const std::string& label) {
    std::vector<uint8_t> info(label.begin(), label.end());
    info.push_back(0x00);
    return info;
}

}  // namespace

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
    // RFC 8291 Section 3.3: Key derivation using Web Push context
    const auto context = build_webpush_context(client_public_key, server_public_key);
    spdlog::debug("WebPush context size: {} bytes", context.size());

    // Step 1: Derive PRK from shared secret and auth secret using the Web Push context
    const std::vector<uint8_t> prk = hkdf::derive(shared_secret, auth_secret, context, 32);
    spdlog::debug("PRK size: {} bytes", prk.size());

    // Step 2: Derive content encryption key (16 bytes for AES-128)
    const auto key_info = build_content_encoding_label("Content-Encoding: aes128gcm");
    spdlog::debug("Key info size: {} bytes", key_info.size());
    content_encryption_key = hkdf::derive(prk, salt, key_info, 16);

    // Step 3: Derive nonce (12 bytes for AES-GCM)
    const auto nonce_info = build_content_encoding_label("Content-Encoding: nonce");
    spdlog::debug("Nonce info size: {} bytes", nonce_info.size());
    nonce = hkdf::derive(prk, salt, nonce_info, 12);
}

webpush_encryption::encrypted_payload webpush_encryption::encrypt(
    const std::string& plaintext,
    const push_subscription& subscription
) {
    spdlog::debug("Encrypting Web Push payload of {} bytes", plaintext.size());

    // Step 1: Decode the client's public key (p256dh) from base64url
    const std::vector<uint8_t> client_public_key = base64url::decode(subscription.p256dh);
    if (client_public_key.size() != 65) {
        throw std::runtime_error("Invalid client public key size: " + std::to_string(client_public_key.size()));
    }
    spdlog::debug("Client public key: {} bytes", client_public_key.size());

    // Step 2: Generate a new ephemeral ECDH key pair (server keys)
    std::vector<uint8_t> server_public_key;
    std::vector<uint8_t> server_private_key;
    if (!ecdh::generate_keypair(server_public_key, server_private_key)) {
        throw std::runtime_error("Failed to generate ECDH key pair");
    }
    spdlog::debug("Generated server key pair: public={} bytes, private={} bytes", server_public_key.size(), server_private_key.size());

    // Step 3: Compute shared secret using ECDH
    std::vector<uint8_t> shared_secret;
    if (!ecdh::compute_shared_secret(server_private_key, client_public_key, shared_secret)) {
        throw std::runtime_error("Failed to compute ECDH shared secret");
    }
    spdlog::debug("Computed shared secret: {} bytes", shared_secret.size());

    // Step 4: Decode the auth secret from base64url
    const std::vector<uint8_t> auth_secret = base64url::decode(subscription.auth);
    if (auth_secret.size() != 16) {
        throw std::runtime_error("Invalid auth secret size: " + std::to_string(auth_secret.size()));
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
    spdlog::debug("Derived CEK: {} bytes, nonce: {} bytes", content_encryption_key.size(), nonce.size());

    // Step 7: Prepare plaintext with padding
    // RFC 8291 Format: plaintext || 0x02 || padding_bytes
    // The delimiter 0x02 marks the end of content, followed by optional padding
    std::vector<uint8_t> padded_plaintext(plaintext.begin(), plaintext.end());
    padded_plaintext.push_back(0x02);  // Delimiter marking end of content
    // No additional padding bytes needed (padding is optional after the delimiter)

    spdlog::debug("Padded plaintext: {} bytes (original: {} bytes)", padded_plaintext.size(), plaintext.size());

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
    constexpr uint32_t record_size = 4096;
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

    spdlog::debug("Built request body: {} bytes total (salt=16, header=5, key=65, ciphertext={})", body.size(), payload.ciphertext.size());

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
