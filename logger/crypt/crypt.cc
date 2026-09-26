#include "crypt.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/param_build.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <stdexcept>
#include <utility>
#include <vector>

namespace logger {
namespace crypt {

namespace {

std::string GetOpenSSLError() {
  unsigned long error = ERR_get_error();

  if (error == 0) {
    return "Unknown OpenSSL error";
  }

  char buffer[256] = {};
  ERR_error_string_n(error, buffer, sizeof(buffer));

  return std::string(buffer);
}

} // namespace

std::string BinaryKeyToHex(const std::string &binary_key) {
  static constexpr char kHex[] = "0123456789abcdef";

  std::string hex_key;
  hex_key.reserve(binary_key.size() * 2);

  for (unsigned char byte : binary_key) {
    hex_key.push_back(kHex[(byte >> 4) & 0x0F]);
    hex_key.push_back(kHex[byte & 0x0F]);
  }

  return hex_key;
}

std::string HexKeyToBinary(const std::string &hex_key) {
  if (hex_key.size() % 2 != 0) {
    throw std::runtime_error("Invalid hex key: odd number of characters");
  }

  auto HexValue = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }

    return -1;
  };

  std::string binary_key;
  binary_key.reserve(hex_key.size() / 2);

  for (size_t i = 0; i < hex_key.size(); i += 2) {
    const int high = HexValue(hex_key[i]);
    const int low = HexValue(hex_key[i + 1]);

    if (high < 0 || low < 0) {
      throw std::runtime_error("Invalid hex key");
    }

    binary_key.push_back(static_cast<char>((high << 4) | low));
  }

  return binary_key;
}

std::tuple<std::string, std::string> GenECDHKey() {
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);

  if (ctx == nullptr) {
    throw std::runtime_error("Failed to create ECDH key context: " +
                             GetOpenSSLError());
  }

  if (EVP_PKEY_keygen_init(ctx) <= 0) {
    EVP_PKEY_CTX_free(ctx);

    throw std::runtime_error("Failed to initialize ECDH key generation: " +
                             GetOpenSSLError());
  }

  if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) <= 0) {
    EVP_PKEY_CTX_free(ctx);

    throw std::runtime_error("Failed to set secp256r1 curve: " +
                             GetOpenSSLError());
  }

  EVP_PKEY *key = nullptr;

  if (EVP_PKEY_keygen(ctx, &key) <= 0) {
    EVP_PKEY_CTX_free(ctx);

    throw std::runtime_error("Failed to generate ECDH key pair: " +
                             GetOpenSSLError());
  }

  EVP_PKEY_CTX_free(ctx);

  // ------------------------------------------------------------
  // Export private key as a raw 32-byte big-endian scalar.
  // ------------------------------------------------------------

  BIGNUM *d = nullptr;

  if (EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_PRIV_KEY, &d) <= 0) {
    EVP_PKEY_free(key);

    throw std::runtime_error("Failed to get private key scalar: " +
                             GetOpenSSLError());
  }

  std::string private_key(BN_num_bytes(d), '\0');

  BN_bn2bin(d, reinterpret_cast<unsigned char *>(private_key.data()));

  BN_free(d);

  // ------------------------------------------------------------
  // Export public key as a raw uncompressed point: 0x04 || X || Y
  // (65 bytes for the prime256v1 / secp256r1 curve).
  // ------------------------------------------------------------

  size_t public_key_size = 0;

  if (EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0,
                                      &public_key_size) <= 0) {
    EVP_PKEY_free(key);

    throw std::runtime_error("Failed to determine public key size: " +
                             GetOpenSSLError());
  }

  std::string public_key(public_key_size, '\0');

  if (EVP_PKEY_get_octet_string_param(
          key, OSSL_PKEY_PARAM_PUB_KEY,
          reinterpret_cast<unsigned char *>(public_key.data()), public_key_size,
          &public_key_size) <= 0) {
    EVP_PKEY_free(key);

    throw std::runtime_error("Failed to export public key point: " +
                             GetOpenSSLError());
  }

  EVP_PKEY_free(key);

  return {std::move(private_key), std::move(public_key)};
}

std::string GenECDHSharedSecret(const std::string &private_key,
                                const std::string &peer_public_key) {
  if (private_key.size() != 32) {
    throw std::runtime_error("Invalid private key size (expected 32 bytes)");
  }

  if (peer_public_key.size() != 65 ||
      static_cast<unsigned char>(peer_public_key[0]) != 0x04) {
    throw std::runtime_error(
        "Invalid peer public key (expected 65-byte uncompressed point)");
  }

  // ------------------------------------------------------------
  // Decode private key (raw 32-byte scalar).
  // ------------------------------------------------------------

  BIGNUM *d = BN_bin2bn(
      reinterpret_cast<const unsigned char *>(private_key.data()),
      static_cast<int>(private_key.size()), nullptr);

  if (d == nullptr) {
    throw std::runtime_error("Failed to decode private key scalar");
  }

  OSSL_PARAM_BLD *local_bld = OSSL_PARAM_BLD_new();

  if (local_bld == nullptr) {
    BN_free(d);

    throw std::runtime_error("Failed to allocate private key param builder");
  }

  if (OSSL_PARAM_BLD_push_utf8_string(local_bld, OSSL_PKEY_PARAM_GROUP_NAME,
                                      SN_X9_62_prime256v1, 0) <= 0 ||
      OSSL_PARAM_BLD_push_BN(local_bld, OSSL_PKEY_PARAM_PRIV_KEY, d) <= 0) {
    OSSL_PARAM_BLD_free(local_bld);
    BN_free(d);

    throw std::runtime_error("Failed to build private key params");
  }

  OSSL_PARAM *local_params = OSSL_PARAM_BLD_to_param(local_bld);

  OSSL_PARAM_BLD_free(local_bld);
  BN_free(d);

  if (local_params == nullptr) {
    throw std::runtime_error("Failed to finalize private key params");
  }

  EVP_PKEY_CTX *local_ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
  EVP_PKEY *local_key = nullptr;

  if (local_ctx == nullptr ||
      EVP_PKEY_fromdata_init(local_ctx) <= 0 ||
      EVP_PKEY_fromdata(local_ctx, &local_key, EVP_PKEY_KEYPAIR,
                        local_params) <= 0) {
    EVP_PKEY_CTX_free(local_ctx);
    OSSL_PARAM_free(local_params);

    throw std::runtime_error("Failed to decode private key: " +
                             GetOpenSSLError());
  }

  EVP_PKEY_CTX_free(local_ctx);
  OSSL_PARAM_free(local_params);

  // ------------------------------------------------------------
  // Decode peer public key (raw 65-byte uncompressed point).
  // ------------------------------------------------------------

  OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();

  if (bld == nullptr) {
    EVP_PKEY_free(local_key);

    throw std::runtime_error("Failed to allocate param builder");
  }

  if (OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME,
                                      SN_X9_62_prime256v1, 0) <= 0 ||
      OSSL_PARAM_BLD_push_octet_string(
          bld, OSSL_PKEY_PARAM_PUB_KEY, peer_public_key.data(),
          peer_public_key.size()) <= 0) {
    OSSL_PARAM_BLD_free(bld);
    EVP_PKEY_free(local_key);

    throw std::runtime_error("Failed to build peer public key params");
  }

  OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(bld);

  OSSL_PARAM_BLD_free(bld);

  if (params == nullptr) {
    EVP_PKEY_free(local_key);

    throw std::runtime_error("Failed to finalize peer public key params");
  }

  EVP_PKEY_CTX *peer_ctx =
      EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
  EVP_PKEY *peer_key = nullptr;

  if (peer_ctx == nullptr ||
      EVP_PKEY_fromdata_init(peer_ctx) <= 0 ||
      EVP_PKEY_fromdata(peer_ctx, &peer_key, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
    EVP_PKEY_CTX_free(peer_ctx);
    OSSL_PARAM_free(params);
    EVP_PKEY_free(local_key);

    throw std::runtime_error("Failed to decode peer public key: " +
                             GetOpenSSLError());
  }

  EVP_PKEY_CTX_free(peer_ctx);
  OSSL_PARAM_free(params);

  // ------------------------------------------------------------
  // Create ECDH derive context.
  // ------------------------------------------------------------

  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(local_key, nullptr);

  if (ctx == nullptr) {
    EVP_PKEY_free(local_key);
    EVP_PKEY_free(peer_key);

    throw std::runtime_error("Failed to create ECDH derive context: " +
                             GetOpenSSLError());
  }

  if (EVP_PKEY_derive_init(ctx) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(local_key);
    EVP_PKEY_free(peer_key);

    throw std::runtime_error("Failed to initialize ECDH derive: " +
                             GetOpenSSLError());
  }

  if (EVP_PKEY_derive_set_peer(ctx, peer_key) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(local_key);
    EVP_PKEY_free(peer_key);

    throw std::runtime_error("Failed to set ECDH peer key: " +
                             GetOpenSSLError());
  }

  // ------------------------------------------------------------
  // Determine shared secret size.
  // ------------------------------------------------------------

  size_t shared_secret_size = 0;

  if (EVP_PKEY_derive(ctx, nullptr, &shared_secret_size) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(local_key);
    EVP_PKEY_free(peer_key);

    throw std::runtime_error("Failed to determine shared secret size: " +
                             GetOpenSSLError());
  }

  std::string shared_secret(shared_secret_size, '\0');

  if (EVP_PKEY_derive(ctx,
                      reinterpret_cast<unsigned char *>(shared_secret.data()),
                      &shared_secret_size) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(local_key);
    EVP_PKEY_free(peer_key);

    throw std::runtime_error("Failed to generate ECDH shared secret: " +
                             GetOpenSSLError());
  }

  shared_secret.resize(shared_secret_size);

  EVP_PKEY_CTX_free(ctx);
  EVP_PKEY_free(local_key);
  EVP_PKEY_free(peer_key);

  return shared_secret;
}

} // namespace crypt
} // namespace logger