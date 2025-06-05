#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <randomx.h>
#include <array>

/**
 * @file hash.h
 * Hashing RandomX EXCLUSIVO para minería real de Monero (XMR).
 * Esta API NO se utiliza para validar resultados de trabajo en pools ni para logging.
 * 
 * Si necesitas hashing para logs/integridad de archivos, usa SHA-3-256 o BLAKE2b,
 * **NUNCA MD5 ni SHA1** (NO SE EXPONE EN ESTE HEADER).
 */

namespace core {

/**
 * Configuración de la instancia RandomX.
 */
struct RandomXConfig {
    randomx_flags flags = randomx_flags::RANDOMX_FLAG_DEFAULT;
    size_t cacheSizeMB = 256;
    bool fullMemory = true;  // Usar modo full (minería real)
};

/**
 * Singleton para gestión global del dataset RandomX.
 */
class RandomXContext {
public:
    // Prohibir copias
    RandomXContext(const RandomXContext&) = delete;
    RandomXContext& operator=(const RandomXContext&) = delete;

    static RandomXContext& getInstance();

    void initialize(const std::vector<uint8_t>& key, const RandomXConfig& config = {});
    void cleanup();

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
    bool m_initialized = false;
};

/**
 * VM dedicada a un hilo de minado.
 * Seguro en concurrencia, reciclable.
 */
class RandomXVM {
public:
    explicit RandomXVM(const RandomXConfig& config = RandomXConfig());
    ~RandomXVM();

    // Prohibir copias
    RandomXVM(const RandomXVM&) = delete;
    RandomXVM& operator=(const RandomXVM&) = delete;

    /**
     * Hashing de un blob binario (76 bytes Monero) + nonce.
     * @param data Bloque de datos (incluye nonce en la posición correspondiente)
     * @return Digest (32 bytes)
     */
    std::array<uint8_t, 32> calculateHash(std::span<const uint8_t> data) const;

    randomx_vm* get();

private:
    randomx_vm* m_vm = nullptr;
};

/**
 * Hash único de Monero para un bloque de datos, usando VM global por hilo.
 * @param data Datos a hashear (ejemplo: blob de trabajo + nonce)
 * @return Hash XMR de 32 bytes (para minería real, nunca para logs)
 */
std::array<uint8_t, 32> monero_hash(std::span<const uint8_t> data);

} // namespace core
