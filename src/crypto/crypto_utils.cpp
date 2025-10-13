#include "crypto_utils.h"
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstring>

namespace argus::crypto {

// ============================================================================
// Base64URL Implementation
// ============================================================================

std::string base64url::encode(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return "";
    }

    // Standard base64 encode
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);

    BUF_MEM* buffer_ptr;
    BIO_get_mem_ptr(bio, &buffer_ptr);
    std::string result(buffer_ptr->data, buffer_ptr->length);
    BIO_free_all(bio);

    // Convert to URL-safe: + -> -, / -> _, remove padding =
    for (char& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    std::erase(result, '=');

    return result;
}

std::string base64url::encode(const std::string& str) {
    const std::vector<uint8_t> data(str.begin(), str.end());
    return encode(data);
}

std::vector<uint8_t> base64url::decode(const std::string& str) {
    if (str.empty()) {
        return {};
    }

    // Convert from URL-safe to standard base64
    std::string b64 = str;
    for (char& c : b64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }

    // Add padding if needed
    while (b64.length() % 4 != 0) {
        b64 += '=';
    }

    // Decode
    BIO* b64_bio = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new_mem_buf(b64.data(), static_cast<int>(b64.length()));
    bio = BIO_push(b64_bio, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> result(b64.length());
    const int decoded_length = BIO_read(bio, result.data(), static_cast<int>(result.size()));
    BIO_free_all(bio);

    if (decoded_length > 0) {
        result.resize(decoded_length);
    } else {
        result.clear();
    }

    return result;
}

std::string base64url::decode_string(const std::string& str) {
    auto data = decode(str);
    return std::string(data.begin(), data.end());
}

// ============================================================================
// ECDH Implementation
// ============================================================================

bool ecdh::generate_keypair(std::vector<uint8_t>& public_key, std::vector<uint8_t>& private_key) {
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!pctx) {
        spdlog::error("Failed to create EVP_PKEY_CTX");
        return false;
    }

    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        spdlog::error("Failed to initialize key generation");
        EVP_PKEY_CTX_free(pctx);
        return false;
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) <= 0) {
        spdlog::error("Failed to set P-256 curve");
        EVP_PKEY_CTX_free(pctx);
        return false;
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) {
        spdlog::error("Failed to generate key pair");
        EVP_PKEY_CTX_free(pctx);
        return false;
    }
    EVP_PKEY_CTX_free(pctx);

    // Extract public key (uncompressed format: 0x04 || X || Y) using modern API
    public_key.resize(65);
    size_t pub_len = 65;
    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                         public_key.data(), 65, &pub_len) != 1) {
        spdlog::error("Failed to export public key, got {} bytes", pub_len);
        EVP_PKEY_free(pkey);
        return false;
    }
    public_key.resize(pub_len);

    // Extract private key (32 bytes) using modern API
    BIGNUM* priv_bn = nullptr;
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY, &priv_bn) != 1 || !priv_bn) {
        spdlog::error("Failed to export private key");
        EVP_PKEY_free(pkey);
        return false;
    }

    private_key.resize(32);
    const int bn_len = BN_bn2binpad(priv_bn, private_key.data(), 32);
    BN_free(priv_bn);

    if (bn_len != 32) {
        spdlog::error("Failed to convert private key to binary: {} bytes", bn_len);
        EVP_PKEY_free(pkey);
        return false;
    }

    EVP_PKEY_free(pkey);
    return true;
}

bool ecdh::compute_shared_secret(
    const std::vector<uint8_t>& private_key,
    const std::vector<uint8_t>& peer_public_key,
    std::vector<uint8_t>& shared_secret
) {
    if (private_key.size() != 32 || peer_public_key.size() != 65) {
        spdlog::error("Invalid key sizes: private={}, public={}", private_key.size(), peer_public_key.size());
        return false;
    }

    // Build EVP_PKEY from private key using OSSL_PARAM_BLD (safer API)
    BIGNUM* priv_bn = BN_bin2bn(private_key.data(), 32, nullptr);
    if (!priv_bn) {
        spdlog::error("Failed to create BIGNUM from private key");
        return false;
    }

    OSSL_PARAM_BLD* param_bld = OSSL_PARAM_BLD_new();
    if (!param_bld) {
        spdlog::error("Failed to create OSSL_PARAM_BLD");
        BN_free(priv_bn);
        return false;
    }

    if (!OSSL_PARAM_BLD_push_utf8_string(param_bld, OSSL_PKEY_PARAM_GROUP_NAME, "prime256v1", 0) ||
        !OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_PRIV_KEY, priv_bn)) {
        spdlog::error("Failed to build params");
        OSSL_PARAM_BLD_free(param_bld);
        BN_free(priv_bn);
        return false;
    }

    OSSL_PARAM* params = OSSL_PARAM_BLD_to_param(param_bld);
    OSSL_PARAM_BLD_free(param_bld);
    BN_free(priv_bn);

    if (!params) {
        spdlog::error("Failed to convert param builder to params");
        return false;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (!ctx) {
        spdlog::error("Failed to create EVP_PKEY_CTX");
        OSSL_PARAM_free(params);
        return false;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0) {
        spdlog::error("Failed to init fromdata");
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        return false;
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0) {
        spdlog::error("Failed to create EVP_PKEY from private key");
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        return false;
    }

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(params);

    // Build peer EVP_PKEY from public key using OSSL_PARAM_BLD
    OSSL_PARAM_BLD* peer_param_bld = OSSL_PARAM_BLD_new();
    if (!peer_param_bld) {
        spdlog::error("Failed to create peer OSSL_PARAM_BLD");
        EVP_PKEY_free(pkey);
        return false;
    }

    if (!OSSL_PARAM_BLD_push_utf8_string(peer_param_bld, OSSL_PKEY_PARAM_GROUP_NAME, "prime256v1", 0) ||
        !OSSL_PARAM_BLD_push_octet_string(peer_param_bld, OSSL_PKEY_PARAM_PUB_KEY,
                                           peer_public_key.data(), peer_public_key.size())) {
        spdlog::error("Failed to build peer params");
        OSSL_PARAM_BLD_free(peer_param_bld);
        EVP_PKEY_free(pkey);
        return false;
    }

    OSSL_PARAM* peer_params = OSSL_PARAM_BLD_to_param(peer_param_bld);
    OSSL_PARAM_BLD_free(peer_param_bld);

    if (!peer_params) {
        spdlog::error("Failed to convert peer param builder to params");
        EVP_PKEY_free(pkey);
        return false;
    }

    EVP_PKEY_CTX* peer_ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (!peer_ctx) {
        spdlog::error("Failed to create peer EVP_PKEY_CTX");
        OSSL_PARAM_free(peer_params);
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_PKEY_fromdata_init(peer_ctx) <= 0) {
        spdlog::error("Failed to init peer fromdata");
        EVP_PKEY_CTX_free(peer_ctx);
        OSSL_PARAM_free(peer_params);
        EVP_PKEY_free(pkey);
        return false;
    }

    EVP_PKEY* peer_pkey = nullptr;
    if (EVP_PKEY_fromdata(peer_ctx, &peer_pkey, EVP_PKEY_PUBLIC_KEY, peer_params) <= 0) {
        spdlog::error("Failed to create peer EVP_PKEY from public key");
        EVP_PKEY_CTX_free(peer_ctx);
        OSSL_PARAM_free(peer_params);
        EVP_PKEY_free(pkey);
        return false;
    }

    EVP_PKEY_CTX_free(peer_ctx);
    OSSL_PARAM_free(peer_params);

    // Compute shared secret using EVP_PKEY_derive
    EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!derive_ctx) {
        spdlog::error("Failed to create derive context");
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_PKEY_derive_init(derive_ctx) <= 0) {
        spdlog::error("Failed to init derive");
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_PKEY_derive_set_peer(derive_ctx, peer_pkey) <= 0) {
        spdlog::error("Failed to set peer key");
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(pkey);
        return false;
    }

    size_t secret_len;
    if (EVP_PKEY_derive(derive_ctx, nullptr, &secret_len) <= 0) {
        spdlog::error("Failed to determine shared secret length");
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(pkey);
        return false;
    }

    shared_secret.resize(secret_len);
    if (EVP_PKEY_derive(derive_ctx, shared_secret.data(), &secret_len) <= 0) {
        spdlog::error("ECDH computation failed");
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(pkey);
        return false;
    }

    shared_secret.resize(secret_len);

    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(peer_pkey);
    EVP_PKEY_free(pkey);

    return true;
}

// ============================================================================
// HKDF Implementation
// ============================================================================

std::vector<uint8_t> hkdf::derive(
    const std::vector<uint8_t>& ikm,
    const std::vector<uint8_t>& salt,
    const std::vector<uint8_t>& info,
    size_t length
) {
    std::vector<uint8_t> result(length);

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) {
        throw std::runtime_error("Failed to create HKDF context");
    }

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to initialize HKDF");
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF hash");
    }

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(), static_cast<int>(salt.size())) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF salt");
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm.data(), static_cast<int>(ikm.size())) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF key");
    }

    if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info.data(), static_cast<int>(info.size())) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF info");
    }

    size_t outlen = length;
    if (EVP_PKEY_derive(pctx, result.data(), &outlen) <= 0 || outlen != length) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("HKDF derivation failed");
    }

    EVP_PKEY_CTX_free(pctx);
    return result;
}

std::vector<uint8_t> hkdf::extract(
    const std::vector<uint8_t>& ikm,
    const std::vector<uint8_t>& salt
) {
    // HKDF-Extract: PRK = HMAC-SHA256(salt, ikm)
    std::vector<uint8_t> prk(32); // SHA-256 output is 32 bytes

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) {
        throw std::runtime_error("Failed to create HKDF context");
    }

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to initialize HKDF");
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF hash");
    }

    if (EVP_PKEY_CTX_set_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF mode to extract");
    }

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(), static_cast<int>(salt.size())) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF salt");
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm.data(), static_cast<int>(ikm.size())) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF key");
    }

    size_t outlen = 32;
    if (EVP_PKEY_derive(pctx, prk.data(), &outlen) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("HKDF extract failed");
    }

    EVP_PKEY_CTX_free(pctx);
    return prk;
}

std::vector<uint8_t> hkdf::expand(
    const std::vector<uint8_t>& prk,
    const std::vector<uint8_t>& info,
    size_t length
) {
    // HKDF-Expand: Expand PRK to desired length using info
    std::vector<uint8_t> result(length);

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) {
        throw std::runtime_error("Failed to create HKDF context");
    }

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to initialize HKDF");
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF hash");
    }

    if (EVP_PKEY_CTX_set_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF mode to expand");
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, prk.data(), static_cast<int>(prk.size())) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF key");
    }

    if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info.data(), static_cast<int>(info.size())) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF info");
    }

    size_t outlen = length;
    if (EVP_PKEY_derive(pctx, result.data(), &outlen) <= 0 || outlen != length) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("HKDF expand failed");
    }

    EVP_PKEY_CTX_free(pctx);
    return result;
}

// ============================================================================
// AES-GCM Implementation
// ============================================================================

bool aes_gcm::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce,
    std::vector<uint8_t>& ciphertext
) {
    if (key.size() != 16 || nonce.size() != 12) {
        spdlog::error("Invalid key or nonce size: key={}, nonce={}", key.size(), nonce.size());
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        spdlog::error("Failed to create cipher context");
        return false;
    }

    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1) {
        spdlog::error("Failed to initialize AES-GCM");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // Set nonce length (12 bytes)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) {
        spdlog::error("Failed to set nonce length");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // Set key and nonce
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
        spdlog::error("Failed to set key and nonce");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // Encrypt plaintext
    ciphertext.resize(plaintext.size() + 16); // +16 for authentication tag
    int len;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1) {
        spdlog::error("Encryption failed");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    int ciphertext_len = len;

    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        spdlog::error("Encryption finalization failed");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    ciphertext_len += len;

    // Get authentication tag (16 bytes)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                            ciphertext.data() + ciphertext_len) != 1) {
        spdlog::error("Failed to get authentication tag");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    ciphertext.resize(ciphertext_len + 16);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// ============================================================================
// ECDSA Implementation
// ============================================================================

static std::vector<unsigned char> b64url_decode_strict(const std::string& s) {
    std::string t; t.reserve(s.size()+2);
    for (const char c : s) t.push_back(c=='-'?'+':(c=='_'?'/':c));
    const int pad = (4 - static_cast<int>(t.size())%4) % 4; t.append(pad, '=');
    std::vector<unsigned char> out((t.size()/4)*3);
    int n = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(t.data()), static_cast<int>(t.size()));
    if (n < 0) return {};
    if (pad == 1) n -= 1; else if (pad == 2) n -= 2;
    if (n < 0) return {};
    out.resize(static_cast<size_t>(n));
    return out;
}

static EVP_PKEY* make_p256_from_b64url_scalar_modern(const std::string& priv_b64url) {
    // 1) Decode Base64URL scalar and normalize to 32 bytes (big-endian)
    auto d = b64url_decode_strict(priv_b64url);
    if (d.empty() || d.size() > 32) return nullptr;
    if (d.size() < 32) d.insert(d.begin(), 32 - d.size(), 0x00);

    // 2) Build EVP_PKEY using OSSL_PARAM_BLD (safer API)
    BIGNUM* bn = BN_bin2bn(d.data(), static_cast<int>(d.size()), nullptr);
    if (!bn) return nullptr;

    OSSL_PARAM_BLD* param_bld = OSSL_PARAM_BLD_new();
    if (!param_bld) {
        BN_clear_free(bn);
        return nullptr;
    }

    if (!OSSL_PARAM_BLD_push_utf8_string(param_bld, OSSL_PKEY_PARAM_GROUP_NAME, "prime256v1", 0) ||
        !OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_PRIV_KEY, bn)) {
        OSSL_PARAM_BLD_free(param_bld);
        BN_clear_free(bn);
        return nullptr;
    }

    OSSL_PARAM* params = OSSL_PARAM_BLD_to_param(param_bld);
    OSSL_PARAM_BLD_free(param_bld);
    BN_clear_free(bn);

    if (!params) return nullptr;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (!ctx) {
        OSSL_PARAM_free(params);
        return nullptr;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        return nullptr;
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        return nullptr;
    }

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(params);
    return pkey;
}


bool ecdsa::sign_es256(
    const std::vector<uint8_t>& data,
    const std::string& private_key_pem,
    std::vector<uint8_t>& signature
) {
    // Load private key from base64url-encoded scalar
    EVP_PKEY* pkey = make_p256_from_b64url_scalar_modern(private_key_pem);
    if (!pkey) {
        spdlog::error("Failed to construct EC key from VAPID scalar");
        return false;
    }
    ////////////////////////////////////////////////////////////////
    // Create signing context
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        spdlog::error("Failed to create MD context");
        EVP_PKEY_free(pkey);
        return false;
    }

    // Initialize signing with SHA-256
    if (EVP_DigestSignInit(md_ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
        spdlog::error("Failed to initialize signing");
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return false;
    }

    // Sign the data
    size_t sig_len;
    if (EVP_DigestSign(md_ctx, nullptr, &sig_len, data.data(), data.size()) != 1) {
        spdlog::error("Failed to determine signature length");
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return false;
    }

    std::vector<uint8_t> der_sig(sig_len);
    if (EVP_DigestSign(md_ctx, der_sig.data(), &sig_len, data.data(), data.size()) != 1) {
        spdlog::error("Signing failed");
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return false;
    }
    der_sig.resize(sig_len);

    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);

    // Convert DER signature to raw format (R || S)
    // DER format: 0x30 [total-length] 0x02 [r-length] [r] 0x02 [s-length] [s]
    const uint8_t* p = der_sig.data();
    ECDSA_SIG* ec_sig = d2i_ECDSA_SIG(nullptr, &p, static_cast<long>(der_sig.size()));
    if (!ec_sig) {
        spdlog::error("Failed to parse DER signature");
        return false;
    }

    const BIGNUM* r;
    const BIGNUM* s;
    ECDSA_SIG_get0(ec_sig, &r, &s);

    signature.resize(64);
    BN_bn2binpad(r, signature.data(), 32);
    BN_bn2binpad(s, signature.data() + 32, 32);

    ECDSA_SIG_free(ec_sig);
    return true;
}

// ============================================================================
// HMAC-SHA256 Implementation
// ============================================================================

std::vector<uint8_t> hmac_sha256::compute(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& data
) {
    std::vector<uint8_t> result(32);
    unsigned int len = 32;

    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         data.data(), data.size(), result.data(), &len);

    result.resize(len);
    return result;
}

} // namespace argus::crypto
