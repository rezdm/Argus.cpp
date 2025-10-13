#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace argus::crypto {

/**
 * Base64 URL-safe encoding/decoding utilities
 */
class base64url {
public:
    /**
     * Encode binary data to base64url format
     * @param data Binary data to encode
     * @return Base64url encoded string
     */
    static std::string encode(const std::vector<uint8_t>& data);

    /**
     * Encode string to base64url format
     * @param str String to encode
     * @return Base64url encoded string
     */
    static std::string encode(const std::string& str);

    /**
     * Decode base64url string to binary data
     * @param str Base64url encoded string
     * @return Decoded binary data
     */
    static std::vector<uint8_t> decode(const std::string& str);

    /**
     * Decode base64url string to string
     * @param str Base64url encoded string
     * @return Decoded string
     */
    static std::string decode_string(const std::string& str);
};

/**
 * ECDH (Elliptic Curve Diffie-Hellman) utilities for Web Push encryption
 */
class ecdh {
public:
    /**
     * Generate a new ECDH key pair on the P-256 curve
     * @param public_key Output: 65-byte uncompressed public key (0x04 + X + Y)
     * @param private_key Output: 32-byte private key
     * @return true on success, false on failure
     */
    static bool generate_keypair(std::vector<uint8_t>& public_key, std::vector<uint8_t>& private_key);

    /**
     * Compute shared secret using ECDH
     * @param private_key Our 32-byte private key
     * @param peer_public_key Peer's 65-byte uncompressed public key
     * @param shared_secret Output: 32-byte shared secret
     * @return true on success, false on failure
     */
    static bool compute_shared_secret(
        const std::vector<uint8_t>& private_key,
        const std::vector<uint8_t>& peer_public_key,
        std::vector<uint8_t>& shared_secret
    );
};

/**
 * HKDF (HMAC-based Key Derivation Function) for Web Push
 */
class hkdf {
public:
    /**
     * Derive keys using HKDF-SHA256
     * @param ikm Input keying material
     * @param salt Salt value
     * @param info Context/info string
     * @param length Output key length in bytes
     * @return Derived key
     */
    static std::vector<uint8_t> derive(
        const std::vector<uint8_t>& ikm,
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& info,
        size_t length
    );

    /**
     * HKDF-Extract: Derive a pseudorandom key from input keying material
     * @param ikm Input keying material
     * @param salt Salt value
     * @return Pseudorandom key (PRK)
     */
    static std::vector<uint8_t> extract(
        const std::vector<uint8_t>& ikm,
        const std::vector<uint8_t>& salt
    );

    /**
     * HKDF-Expand: Expand a pseudorandom key to desired length
     * @param prk Pseudorandom key from extract
     * @param info Context/info string
     * @param length Output key length in bytes
     * @return Derived key
     */
    static std::vector<uint8_t> expand(
        const std::vector<uint8_t>& prk,
        const std::vector<uint8_t>& info,
        size_t length
    );
};

/**
 * AES-GCM encryption for Web Push payloads
 */
class aes_gcm {
public:
    /**
     * Encrypt data using AES-128-GCM
     * @param plaintext Data to encrypt
     * @param key 16-byte encryption key
     * @param nonce 12-byte nonce
     * @param ciphertext Output: encrypted data + 16-byte authentication tag
     * @return true on success, false on failure
     */
    static bool encrypt(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce,
        std::vector<uint8_t>& ciphertext
    );
};

/**
 * ECDSA signing for VAPID JWT tokens
 */
class ecdsa {
public:
    /**
     * Sign data using ECDSA with P-256 and SHA-256
     * @param data Data to sign
     * @param private_key_pem PEM-encoded ES256 private key
     * @param signature Output: 64-byte signature (R || S)
     * @return true on success, false on failure
     */
    static bool sign_es256(
        const std::vector<uint8_t>& data,
        const std::string& private_key_pem,
        std::vector<uint8_t>& signature
    );
};

/**
 * HMAC-SHA256 for various Web Push operations
 */
class hmac_sha256 {
public:
    /**
     * Compute HMAC-SHA256
     * @param key HMAC key
     * @param data Data to authenticate
     * @return 32-byte HMAC
     */
    static std::vector<uint8_t> compute(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& data
    );
};

} // namespace argus::crypto
