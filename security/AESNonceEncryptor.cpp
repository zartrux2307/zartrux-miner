#include "AESNonceEncryptor.h"
#include <openssl/rand.h>
#include <openssl/err.h>
#include <stdexcept>
#include <cstring>

namespace zartrux::security {

AESNonceEncryptor::AESNonceEncryptor(const std::string& key) {
    key_ = deriveKey(key);
}

std::vector<uint8_t> AESNonceEncryptor::deriveKey(const std::string& input) {
    std::vector<uint8_t> result(AES_KEY_SIZE, 0);
    size_t len = std::min(input.size(), AES_KEY_SIZE);
    std::memcpy(result.data(), input.data(), len);
    return result;
}

std::vector<uint8_t> AESNonceEncryptor::generateRandomIV() {
    std::vector<uint8_t> iv(AES_BLOCK_SIZE);
    if (RAND_bytes(iv.data(), AES_BLOCK_SIZE) != 1) {
        throw std::runtime_error("❌ Error generando IV aleatorio");
    }
    return iv;
}

std::vector<uint8_t> AESNonceEncryptor::encrypt(const std::string& plaintext) {
    std::vector<uint8_t> iv = generateRandomIV();
    std::vector<uint8_t> ciphertext(iv);  // Prepend IV

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("❌ Error creando contexto de cifrado");

    int len = 0;
    int ciphertext_len = 0;
    std::vector<uint8_t> buffer(plaintext.size() + AES_BLOCK_SIZE);

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("❌ Error en EncryptInit");
    }

    if (EVP_EncryptUpdate(ctx, buffer.data(), &len, reinterpret_cast<const uint8_t*>(plaintext.data()), static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("❌ Error en EncryptUpdate");
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, buffer.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("❌ Error en EncryptFinal");
    }
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.insert(ciphertext.end(), buffer.begin(), buffer.begin() + ciphertext_len);
    return ciphertext;
}

std::string AESNonceEncryptor::decrypt(const std::vector<uint8_t>& ciphertext) {
    if (ciphertext.size() < AES_BLOCK_SIZE)
        throw std::runtime_error("❌ El texto cifrado es demasiado corto");

    const uint8_t* iv = ciphertext.data();
    const uint8_t* cipher_data = ciphertext.data() + AES_BLOCK_SIZE;
    size_t cipher_len = ciphertext.size() - AES_BLOCK_SIZE;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("❌ Error creando contexto de descifrado");

    std::vector<uint8_t> buffer(cipher_len + AES_BLOCK_SIZE);
    int len = 0;
    int plaintext_len = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_.data(), iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("❌ Error en DecryptInit");
    }

    if (EVP_DecryptUpdate(ctx, buffer.data(), &len, cipher_data, static_cast<int>(cipher_len)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("❌ Error en DecryptUpdate");
    }
    plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, buffer.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("❌ Error en DecryptFinal: clave incorrecta o datos corruptos");
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return std::string(reinterpret_cast<char*>(buffer.data()), plaintext_len);
}

} // namespace zartrux::security
