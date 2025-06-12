#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <array>

#include "crypto/randomx/randomx.h"

namespace core {

struct RandomXConfig {
    randomx_flags flags = randomx_flags::RANDOMX_FLAG_DEFAULT;
    bool fullMemory = true;
};

class RandomXContext {
public:
    static RandomXContext& getInstance();
    RandomXContext(const RandomXContext&) = delete;
    RandomXContext& operator=(const RandomXContext&) = delete;

    void initialize(const std::vector<uint8_t>& key, const RandomXConfig& config = {});
    void reinitialize(const std::vector<uint8_t>& key, const RandomXConfig& config = {});
    bool isInitialized() const;

    randomx_dataset* dataset();
    randomx_cache* cache();
    const RandomXConfig& getConfig() const;

private:
    RandomXContext();
    ~RandomXContext();
    void destroy();

    std::mutex m_mutex;
    randomx_cache* m_cache = nullptr;
    randomx_dataset* m_dataset = nullptr;
    RandomXConfig m_config;
    std::atomic<bool> m_initialized{false};
};

class RandomXVM {
public:
    explicit RandomXVM(const RandomXConfig& config = RandomXConfig());
    ~RandomXVM();

    RandomXVM(RandomXVM&& other) noexcept;
    RandomXVM& operator=(RandomXVM&& other) noexcept;

    RandomXVM(const RandomXVM&) = delete;
    RandomXVM& operator=(const RandomXVM&) = delete;

    std::array<uint8_t, 32> calculateHash(std::span<const uint8_t> data) const;
    randomx_vm* get() const;

private:
    randomx_vm* m_vm = nullptr;
};

} // namespace core