#pragma once

#include <string>
#include <vector>
#include <openssl/evp.h>

namespace zartrux::security {

/**
 * @brief Utilidad para cifrar y descifrar nonces usando AES-256-CBC.
 */
class AESNonceEncryptor {
public:
    explicit AESNonceEncryptor(const std::string& key);

    std::vector<uint8_t> encrypt(const std::string& plaintext);
    std::string decrypt(const std::vector<uint8_t>& ciphertext);

private:
    std::vector<uint8_t> key_;
    static constexpr size_t AES_KEY_SIZE = 32;  // 256 bits
    static constexpr size_t AES_BLOCK_SIZE = 16;

    std::vector<uint8_t> deriveKey(const std::string& input);
    std::vector<uint8_t> generateRandomIV();
};

} // namespace zartrux::security
