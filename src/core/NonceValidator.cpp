#include "NonceValidator.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <cstring>
#include <stdexcept>
#include <chrono>

// Validación rápida estricta (memcpy < 0)
bool NonceValidator::isValidFast(const hash_t& hash, const hash_t& target) {
    return std::memcmp(hash.data(), target.data(), HASH_SIZE) < 0;
}

// Validación configurable (enum + custom)
bool NonceValidator::isValid(const hash_t& hash, const hash_t& target,
                             const ValidatorConfig& config) {
    switch (config.mode) {
        case CompareMode::STRICT_LESS:
            return isValidFast(hash, target);
        case CompareMode::LESS_EQUAL:
            return !isValidFast(target, hash);
        case CompareMode::CUSTOM:
            if (!config.customCompare.has_value()) {
                throw std::invalid_argument("Custom compare function not provided");
            }
            return config.customCompare.value()(hash, target);
        default:
            throw std::invalid_argument("Invalid compare mode");
    }
}

// Calcula el hash para un nonce y jobBlob usando la VM especificada
NonceValidator::hash_t NonceValidator::calculateHash(randomx_vm* vm,
                                                    std::span<const uint8_t> jobBlob,
                                                    uint64_t nonce,
                                                    const ValidatorConfig& config) {
    if (!vm) throw std::invalid_argument("VM is null");
    if (jobBlob.size() < config.noncePosition + config.nonceSize) {
        throw std::invalid_argument("Job blob too small");
    }
    std::vector<uint8_t> mutableBlob(jobBlob.begin(), jobBlob.end());
    insertNonce(mutableBlob, nonce, config.noncePosition, config.nonceSize, config.nonceEndianness);

    hash_t hash;
    randomx_calculate_hash(vm, mutableBlob.data(), mutableBlob.size(), hash.data());
    return hash;
}

// Inserta el nonce en la posición especificada, endian-aware
void NonceValidator::insertNonce(std::vector<uint8_t>& blob, uint64_t nonce,
                                size_t position, size_t size, Endianness endian) {
    if (position + size > blob.size())
        throw std::out_of_range("Nonce position exceeds blob size");
    if (endian == Endianness::BIG) {
        for (size_t i = 0; i < size; ++i)
            blob[position + i] = static_cast<uint8_t>((nonce >> ((size - 1 - i) * 8)) & 0xFF);
    } else {
        for (size_t i = 0; i < size; ++i)
            blob[position + i] = static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF);
    }
}

NonceValidator::NonceValidator(const ValidatorConfig& config)
    : m_config(config) {}

// Validación para 1 solo nonce (segura, con excepción)
bool NonceValidator::validate(uint64_t nonce, const hash_t& target,
                             std::span<const uint8_t> jobBlob, randomx_vm* vm) const {
    try {
        auto hash = calculateHash(vm, jobBlob, nonce, m_config);
        return isValid(hash, target, m_config);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Validation failed: ") + e.what());
    }
}

// Validación secuencial (legacy)
std::vector<bool> NonceValidator::validateBatch(randomx_vm* vm,
                                              const std::vector<uint64_t>& nonces,
                                              const hash_t& target,
                                              std::span<const uint8_t> jobBlob,
                                              const ValidatorConfig& config) {
    std::vector<bool> results;
    results.reserve(nonces.size());

    std::vector<uint8_t> baseBlob(jobBlob.begin(), jobBlob.end());
    const size_t noncePos = config.noncePosition;
    const size_t nonceSize = config.nonceSize;

    for (auto nonce : nonces) {
        try {
            auto tempBlob = baseBlob;
            insertNonce(tempBlob, nonce, noncePos, nonceSize, config.nonceEndianness);
            hash_t hash;
            randomx_calculate_hash(vm, tempBlob.data(), tempBlob.size(), hash.data());
            results.push_back(isValid(hash, target, config));
        } catch (...) {
            results.push_back(false);
        }
    }
    return results;
}

// VALIDACIÓN PARALELA “master level” – super optimizado para CPUs multicore
std::vector<bool> NonceValidator::validateBatchParallel(
    randomx_vm** vms,  // Array de VMs (uno por hilo)
    size_t num_vms,
    const std::vector<uint64_t>& nonces,
    const hash_t& target,
    std::span<const uint8_t> jobBlob,
    const ValidatorConfig& config) {

    const size_t totalNonces = nonces.size();
    std::vector<bool> results(totalNonces, false);
    if (totalNonces == 0) return results;

    const size_t threadsToUse = std::min({num_vms, config.batchThreads, totalNonces});
    const size_t noncesPerThread = (totalNonces + threadsToUse - 1) / threadsToUse;
    const std::vector<uint8_t> baseBlob(jobBlob.begin(), jobBlob.end());
    const size_t noncePos = config.noncePosition;
    const size_t nonceSize = config.nonceSize;

    std::vector<std::future<void>> futures;
    std::atomic<size_t> nextIndex(0);

    auto worker = [&](randomx_vm* vm) {
        while (true) {
            const size_t start = nextIndex.fetch_add(noncesPerThread);
            if (start >= totalNonces) break;
            const size_t end = std::min(start + noncesPerThread, totalNonces);

            std::vector<uint8_t> tempBlob;
            for (size_t i = start; i < end; ++i) {
                try {
                    tempBlob = baseBlob;
                    const uint64_t nonce = nonces[i];
                    insertNonce(tempBlob, nonce, noncePos, nonceSize, config.nonceEndianness);
                    hash_t hash;
                    randomx_calculate_hash(vm, tempBlob.data(), tempBlob.size(), hash.data());
                    results[i] = isValid(hash, target, config);
                } catch (...) {
                    results[i] = false;
                }
            }
        }
    };

    for (size_t i = 0; i < threadsToUse; ++i) {
        futures.emplace_back(std::async(std::launch::async, worker, vms[i]));
    }

    for (auto& fut : futures) {
        fut.get();
    }

    return results;
}
