#pragma once

#include "push_subscription.h"
#include <string>
#include <vector>
#include <cstdint>

namespace argus {

/**
 * Web Push payload encryption following RFC 8291 (aes128gcm content encoding)
 *
 * This class implements the encryption scheme used for Web Push notifications,
 * which uses ECDH for key agreement and AES-128-GCM for encryption.
 */
class webpush_encryption {
public:
    struct encrypted_payload {
        std::vector<uint8_t> ciphertext;  // Encrypted data
        std::vector<uint8_t> salt;         // 16-byte salt
        std::vector<uint8_t> server_public_key;  // 65-byte uncompressed public key
    };

    /**
     * Encrypt a payload for Web Push
     *
     * @param plaintext The message to encrypt (JSON string)
     * @param subscription The push subscription containing encryption keys
     * @return Encrypted payload with salt and server public key
     * @throws std::runtime_error if encryption fails
     */
    static encrypted_payload encrypt(
        const std::string& plaintext,
        const push_subscription& subscription
    );

    /**
     * Build the complete HTTP request body for aes128gcm content encoding
     *
     * This creates the binary payload format:
     * - salt (16 bytes)
     * - record_size (4 bytes, big-endian, value: 4096)
     * - key_length (1 byte, value: 65)
     * - server_public_key (65 bytes)
     * - ciphertext + tag
     *
     * @param payload Encrypted payload from encrypt()
     * @return Complete binary HTTP body
     */
    static std::vector<uint8_t> build_request_body(const encrypted_payload& payload);

private:
    /**
     * Derive encryption key and nonce using HKDF
     */
    static void derive_keys(
        const std::vector<uint8_t>& shared_secret,
        const std::vector<uint8_t>& auth_secret,
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& client_public_key,
        const std::vector<uint8_t>& server_public_key,
        std::vector<uint8_t>& content_encryption_key,
        std::vector<uint8_t>& nonce
    );

    /**
     * Create the info string for HKDF key derivation
     */
    static std::vector<uint8_t> create_info(
        const std::string& type,
        const std::vector<uint8_t>& client_public_key,
        const std::vector<uint8_t>& server_public_key
    );

    /**
     * Generate random salt (16 bytes)
     */
    static std::vector<uint8_t> generate_salt();
};

} // namespace argus
