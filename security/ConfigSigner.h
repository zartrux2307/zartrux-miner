#pragma once

#include <vector>
#include <string>
#include <openssl/evp.h>

namespace zartrux::security {

/**
 * @brief Clase para firmar y verificar configuraciones usando claves RSA (PEM).
 */
class ConfigSigner {
public:
    explicit ConfigSigner(const std::string& privateKeyPem);
    ~ConfigSigner();

    ConfigSigner(const ConfigSigner&) = delete;
    ConfigSigner& operator=(const ConfigSigner&) = delete;

    std::vector<uint8_t> signConfig(const std::string& configContent);
    
    static bool verifySignature(
        const std::string& configContent,
        const std::vector<uint8_t>& signature,
        const std::string& publicKeyPem);

    static void generateKeyPair(
        std::string& outPrivateKeyPem,
        std::string& outPublicKeyPem,
        unsigned bits = 2048);

private:
    EVP_PKEY* privateKey_ = nullptr;

    static std::vector<uint8_t> computeHash(const std::string& content);
};

} // namespace zartrux::security
