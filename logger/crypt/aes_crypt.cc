#include "aes_crypt.h"

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdexcept>
#include <utility>

namespace logger {
namespace crypt {

namespace detail {

constexpr size_t kAESBlockSize = 16;

std::string GenerateKey() {
  // Keep the original design:
  // generate a 16-byte random AES key.
  std::string key(kAESBlockSize, '\0');

  if (RAND_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size()) !=
      1) {
    throw std::runtime_error("Failed to generate AES key");
  }

  return key;
}

std::string GenerateIV() {
  std::string iv(kAESBlockSize, '\0');

  if (RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), iv.size()) !=
      1) {
    throw std::runtime_error("Failed to generate AES IV");
  }

  return iv;
}

void Encrypt(const void *input, size_t input_size, std::string &output,
             const std::string &key, const std::string &iv) {
  if (key.size() != 16 && key.size() != 24 && key.size() != 32) {
    throw std::runtime_error("Invalid AES key size");
  }

  if (iv.size() != kAESBlockSize) {
    throw std::runtime_error("Invalid AES IV size");
  }

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

  if (ctx == nullptr) {
    throw std::runtime_error("Failed to create AES encryption context");
  }

  const EVP_CIPHER *cipher = nullptr;

  switch (key.size()) {
  case 16:
    cipher = EVP_aes_128_cbc();
    break;

  case 24:
    cipher = EVP_aes_192_cbc();
    break;

  case 32:
    cipher = EVP_aes_256_cbc();
    break;

  default:
    EVP_CIPHER_CTX_free(ctx);

    throw std::runtime_error("Unsupported AES key size");
  }

  if (EVP_EncryptInit_ex(ctx, cipher, nullptr,
                         reinterpret_cast<const unsigned char *>(key.data()),
                         reinterpret_cast<const unsigned char *>(iv.data())) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);

    throw std::runtime_error("Failed to initialize AES encryption");
  }

  // CBC + PKCS#7 padding.
  output.resize(input_size + EVP_CIPHER_block_size(cipher));

  int output_size_1 = 0;
  int output_size_2 = 0;

  if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(output.data()),
                        &output_size_1,
                        reinterpret_cast<const unsigned char *>(input),
                        static_cast<int>(input_size)) != 1) {
    EVP_CIPHER_CTX_free(ctx);

    throw std::runtime_error("Failed to encrypt data");
  }

  if (EVP_EncryptFinal_ex(
          ctx, reinterpret_cast<unsigned char *>(output.data()) + output_size_1,
          &output_size_2) != 1) {
    EVP_CIPHER_CTX_free(ctx);

    throw std::runtime_error("Failed to finalize AES encryption");
  }

  output.resize(output_size_1 + output_size_2);

  EVP_CIPHER_CTX_free(ctx);
}

std::string Decrypt(const void *data, size_t size, const std::string &key,
                    const std::string &iv) {
  if (key.size() != 16 && key.size() != 24 && key.size() != 32) {
    throw std::runtime_error("Invalid AES key size");
  }

  if (iv.size() != kAESBlockSize) {
    throw std::runtime_error("Invalid AES IV size");
  }

  const EVP_CIPHER *cipher = nullptr;

  switch (key.size()) {
  case 16:
    cipher = EVP_aes_128_cbc();
    break;

  case 24:
    cipher = EVP_aes_192_cbc();
    break;

  case 32:
    cipher = EVP_aes_256_cbc();
    break;

  default:
    throw std::runtime_error("Unsupported AES key size");
  }

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

  if (ctx == nullptr) {
    throw std::runtime_error("Failed to create AES decryption context");
  }

  if (EVP_DecryptInit_ex(ctx, cipher, nullptr,
                         reinterpret_cast<const unsigned char *>(key.data()),
                         reinterpret_cast<const unsigned char *>(iv.data())) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);

    throw std::runtime_error("Failed to initialize AES decryption");
  }

  std::string plaintext;
  plaintext.resize(size);

  int output_size_1 = 0;
  int output_size_2 = 0;

  if (EVP_DecryptUpdate(
          ctx, reinterpret_cast<unsigned char *>(plaintext.data()),
          &output_size_1, reinterpret_cast<const unsigned char *>(data),
          static_cast<int>(size)) != 1) {
    EVP_CIPHER_CTX_free(ctx);

    throw std::runtime_error("Failed to decrypt data");
  }

  // EVP_DecryptFinal_ex verifies and removes
  // PKCS#7 padding.
  if (EVP_DecryptFinal_ex(ctx,
                          reinterpret_cast<unsigned char *>(plaintext.data()) +
                              output_size_1,
                          &output_size_2) != 1) {
    EVP_CIPHER_CTX_free(ctx);

    throw std::runtime_error("Failed to finalize AES decryption");
  }

  plaintext.resize(output_size_1 + output_size_2);

  EVP_CIPHER_CTX_free(ctx);

  return plaintext;
}

} // namespace detail

AESCrypt::AESCrypt(std::string key) : key_(std::move(key)) {}

void AESCrypt::Encrypt(const void *input, size_t input_size,
                       std::string &output) {
  // Generate a fresh random IV for every encryption.
  const std::string iv = GenerateIV();

  std::string ciphertext;

  detail::Encrypt(input, input_size, ciphertext, key_, iv);

  // Store:
  //
  //   [ 16-byte IV ][ ciphertext ]
  //
  // IV does not need to be secret.
  output.clear();
  output.reserve(iv.size() + ciphertext.size());

  output.append(iv);
  output.append(ciphertext);
}

std::string AESCrypt::Decrypt(const void *data, size_t size) {
  constexpr size_t kIVSize = detail::kAESBlockSize;

  if (size <= kIVSize) {
    throw std::runtime_error("Invalid AES ciphertext");
  }

  const char *bytes = reinterpret_cast<const char *>(data);

  // The first 16 bytes contain the IV.
  const std::string iv(bytes, kIVSize);

  // The remaining bytes contain ciphertext.
  const void *ciphertext = bytes + kIVSize;

  const size_t ciphertext_size = size - kIVSize;

  return detail::Decrypt(ciphertext, ciphertext_size, key_, iv);
}

std::string AESCrypt::GenerateKey() { return detail::GenerateKey(); }

std::string AESCrypt::GenerateIV() { return detail::GenerateIV(); }

} // namespace crypt
} // namespace logger