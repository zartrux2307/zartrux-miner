#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <functional>
#include <optional>
#include <randomx.h>
#include <span>
#include <thread>
#include <future>
#include <mutex>

class NonceValidator {
public:
    static constexpr size_t HASH_SIZE = 32;
    using hash_t = std::array<uint8_t, HASH_SIZE>;

    enum class CompareMode : uint8_t {
        STRICT_LESS,
        LESS_EQUAL,
        CUSTOM
    };

    enum class Endianness : uint8_t {
        LITTLE,
        BIG
    };

    struct ValidatorConfig {
        CompareMode mode = CompareMode::STRICT_LESS;
        size_t noncePosition = 39;
        size_t nonceSize = 4;
        Endianness nonceEndianness = Endianness::LITTLE;
        bool threadLocalVM = true;
        size_t batchThreads = std::thread::hardware_concurrency();
        std::optional<std::function<bool(const hash_t&, const hash_t&)>> customCompare;
    };

    explicit NonceValidator(const ValidatorConfig& config = ValidatorConfig());

    static bool isValid(const hash_t& hash, const hash_t& target,
                        const ValidatorConfig& config = ValidatorConfig());

    static bool isValidFast(const hash_t& hash, const hash_t& target);

    static hash_t calculateHash(randomx_vm* vm, std::span<const uint8_t> jobBlob,
                                uint64_t nonce, const ValidatorConfig& config = ValidatorConfig());

    bool validate(uint64_t nonce, const hash_t& target,
                 std::span<const uint8_t> jobBlob, randomx_vm* vm) const;

    static std::vector<bool> validateBatch(randomx_vm* vm,
                                         const std::vector<uint64_t>& nonces,
                                         const hash_t& target,
                                         std::span<const uint8_t> jobBlob,
                                         const ValidatorConfig& config = ValidatorConfig());

    // VALIDACIÓN EN PARALELO: Producción, super-eficiente
    static std::vector<bool> validateBatchParallel(
        randomx_vm** vms,  // Array de VMs, uno por hilo
        size_t num_vms,
        const std::vector<uint64_t>& nonces,
        const hash_t& target,
        std::span<const uint8_t> jobBlob,
        const ValidatorConfig& config = ValidatorConfig());

private:
    static void insertNonce(std::vector<uint8_t>& blob, uint64_t nonce,
                           size_t position, size_t size, Endianness endian);

    ValidatorConfig m_config;
};
