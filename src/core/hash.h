#pragma once


#include "crypto/randomx/randomx.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <array>


namespace core {

struct RandomXConfig {
    randomx_flags flags = randomx_flags::RANDOMX_FLAG_DEFAULT;
    bool fullMemory = true;
};

/**
 * @class RandomXContext
 * @brief Singleton para la gestión global del cache y dataset de RandomX.
 * Garantiza que los recursos pesados de RandomX se inicializan una sola vez.
 */
class RandomXContext {
public:
    static RandomXContext& getInstance();

    RandomXContext(const RandomXContext&) = delete;
    RandomXContext& operator=(const RandomXContext&) = delete;

    void initialize(const std::vector<uint8_t>& key, const RandomXConfig& config = {});
    
    // --- MEJORA (Punto 4c): Permite la recarga dinámica de configuración ---
    void reinitialize(const std::vector<uint8_t>& key, const RandomXConfig& config = {});

    randomx_dataset* dataset();
    randomx_cache* cache();
    const RandomXConfig& getConfig() const;
    bool isInitialized() const;

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

/**
 * @class RandomXVM
 * @brief Wrapper para una máquina virtual de RandomX, diseñada para ser usada por un único hilo.
 */
class RandomXVM {
public:
    explicit RandomXVM(const RandomXConfig& config = RandomXConfig());
    ~RandomXVM();

    // --- MEJORA (Punto 4b): Se implementa Move Semantics ---
    RandomXVM(RandomXVM&& other) noexcept;
    RandomXVM& operator=(RandomXVM&& other) noexcept;

    // Se prohíben las copias para evitar duplicación de recursos
    RandomXVM(const RandomXVM&) = delete;
    RandomXVM& operator=(const RandomXVM&) = delete;

    std::array<uint8_t, 32> calculateHash(std::span<const uint8_t> data) const;
    randomx_vm* get() const;

private:
    randomx_vm* m_vm = nullptr;
};

} // namespace core