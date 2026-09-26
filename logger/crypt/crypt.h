#pragma once

#include <cstddef>
#include <string>
#include <tuple>

namespace logger {
namespace crypt {

// Generate an ECDH private key and public key.
std::tuple<std::string, std::string> GenECDHKey();

// Generate ECDH shared secret from private key and peer public key.
std::string GenECDHSharedSecret(const std::string &private_key,
                                const std::string &peer_public_key);

// Convert binary data to hexadecimal string.
std::string BinaryKeyToHex(const std::string &binary_key);

// Convert hexadecimal string to binary data.
std::string HexKeyToBinary(const std::string &hex_key);

class Crypt {
public:
  virtual ~Crypt() = default;

  virtual void Encrypt(const void *input, size_t input_size,
                       std::string &output) = 0;

  virtual std::string Decrypt(const void *data, size_t size) = 0;
};

} // namespace crypt
} // namespace logger