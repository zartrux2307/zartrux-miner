#include "ConfigSigner.h"
#include <openssl/err.h>
#include <openssl/pem.h>
#include <stdexcept>
#include <sstream>
#include <vector>

namespace zartrux::security {

ConfigSigner::ConfigSigner(const std::string& privateKeyPem) {
    BIO* bio = BIO_new_mem_buf(privateKeyPem.data(), static_cast<int>(privateKeyPem.size()));
    if (!bio) throw std::runtime_error("Failed to create BIO for private key");

    privateKey_ = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!privateKey_) {
        throw std::runtime_error("Failed to parse private key: " + std::string(ERR_error_string(ERR_get_error(), nullptr)));
    }
}

ConfigSigner::~ConfigSigner() {
    if (privateKey_) {
        EVP_PKEY_free(privateKey_);
        privateKey_ = nullptr;
    }
}

std::vector<uint8_t> ConfigSigner::computeHash(const std::string& content) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("Failed to create hash context");

    try {
        if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1)
            throw std::runtime_error("Failed to initialize hash");

        if (EVP_DigestUpdate(mdctx, content.data(), content.size()) != 1)
            throw std::runtime_error("Failed to update hash");

        std::vector<uint8_t> hash(EVP_MAX_MD_SIZE);
        unsigned int hashLen = 0;

        if (EVP_DigestFinal_ex(mdctx, hash.data(), &hashLen) != 1)
            throw std::runtime_error("Failed to finalize hash");

        hash.resize(hashLen);
        EVP_MD_CTX_free(mdctx);
        return hash;

    } catch (...) {
        EVP_MD_CTX_free(mdctx);
        throw;
    }
}

std::vector<uint8_t> ConfigSigner::signConfig(const std::string& configContent) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("Failed to create signing context");

    try {
        if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, privateKey_) != 1)
            throw std::runtime_error("Failed to initialize signing");

        if (EVP_DigestSignUpdate(mdctx, configContent.data(), configContent.size()) != 1)
            throw std::runtime_error("Failed to update signing");

        size_t sigLen = 0;
        if (EVP_DigestSignFinal(mdctx, nullptr, &sigLen) != 1)
            throw std::runtime_error("Failed to determine signature length");

        std::vector<uint8_t> signature(sigLen);
        if (EVP_DigestSignFinal(mdctx, signature.data(), &sigLen) != 1)
            throw std::runtime_error("Failed to generate signature");

        signature.resize(sigLen);
        EVP_MD_CTX_free(mdctx);
        return signature;

    } catch (...) {
        EVP_MD_CTX_free(mdctx);
        throw;
    }
}

bool ConfigSigner::verifySignature(
    const std::string& configContent,
    const std::vector<uint8_t>& signature,
    const std::string& publicKeyPem) {

    BIO* bio = BIO_new_mem_buf(publicKeyPem.data(), static_cast<int>(publicKeyPem.size()));
    if (!bio) throw std::runtime_error("Failed to create BIO for public key");

    EVP_PKEY* pubKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!pubKey) throw std::runtime_error("Failed to parse public key");

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pubKey);
        throw std::runtime_error("Failed to create verification context");
    }

    bool success = false;
    try {
        if (EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pubKey) == 1 &&
            EVP_DigestVerifyUpdate(mdctx, configContent.data(), configContent.size()) == 1) {

            success = (EVP_DigestVerifyFinal(mdctx, signature.data(), signature.size()) == 1);
        }

        EVP_PKEY_free(pubKey);
        EVP_MD_CTX_free(mdctx);
        return success;

    } catch (...) {
        EVP_PKEY_free(pubKey);
        EVP_MD_CTX_free(mdctx);
        throw;
    }
}

void ConfigSigner::generateKeyPair(
    std::string& outPrivateKeyPem,
    std::string& outPublicKeyPem,
    unsigned bits) {

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) throw std::runtime_error("Failed to create key generation context");

    try {
        if (EVP_PKEY_keygen_init(ctx) != 1 ||
            EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) != 1)
            throw std::runtime_error("Failed to setup key generation");

        EVP_PKEY* pkey = nullptr;
        if (EVP_PKEY_keygen(ctx, &pkey) != 1)
            throw std::runtime_error("Failed to generate key pair");

        BIO* priBio = BIO_new(BIO_s_mem());
        PEM_write_bio_PrivateKey(priBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
        BUF_MEM* priBuf = nullptr;
        BIO_get_mem_ptr(priBio, &priBuf);
        outPrivateKeyPem.assign(priBuf->data, priBuf->length);
        BIO_free(priBio);

        BIO* pubBio = BIO_new(BIO_s_mem());
        PEM_write_bio_PUBKEY(pubBio, pkey);
        BUF_MEM* pubBuf = nullptr;
        BIO_get_mem_ptr(pubBio, &pubBuf);
        outPublicKeyPem.assign(pubBuf->data, pubBuf->length);
        BIO_free(pubBio);

        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);

    } catch (...) {
        EVP_PKEY_CTX_free(ctx);
        throw;
    }
}

} // namespace zartrux::security
