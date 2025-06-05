#ifndef SMART_CACHE_H
#define SMART_CACHE_H

#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>

/**
 * Gestión avanzada de memoria para buffers de minería y datasets RandomX
 * 
 * Características:
 * - Asignación contigua de nonces sin colisiones
 * - Cache de datasets para múltiples workers
 * - Gestión de memoria no volátil (NVRAM)
 * - Reutilización inteligente de buffers
 */
class SmartCache {
public:
    static SmartCache& getInstance();
    
    // Elimina copias
    SmartCache(const SmartCache&) = delete;
    SmartCache& operator=(const SmartCache&) = delete;
    
    /**
     * Reserva un rango contiguo de nonces
     * @param count Cantidad de nonces a reservar
     * @return Primer nonce del rango reservado
     */
    uint64_t allocateNonceRange(size_t count);
    
    /**
     * Obtiene un dataset de RandomX para minería
     * @param seed Semilla para el dataset
     * @return Puntero al dataset inicializado
     */
    std::shared_ptr<const std::vector<uint8_t>> getDataset(const std::string& seed);
    
    /**
     * Reserva un buffer de trabajo
     * @param size Tamaño requerido en bytes
     * @return Puntero al buffer alineado
     */
    void* allocateWorkBuffer(size_t size, size_t alignment = 64);
    
    /**
     * Libera un buffer de trabajo
     * @param ptr Puntero al buffer a liberar
     */
    void freeWorkBuffer(void* ptr);
    
    /**
     * Estadísticas de uso
     */
    size_t getMemoryUsage() const;
    size_t getCacheHitRate() const;

private:
    SmartCache();
    ~SmartCache();
    
    struct WorkBuffer {
        void* ptr;
        size_t size;
        size_t alignment;
    };
    
    struct DatasetHandle {
        std::string seed;
        std::weak_ptr<std::vector<uint8_t>> weak_ref;
    };
    
    // Miembros
    std::atomic<uint64_t> m_nextNonce;
    mutable std::mutex m_mutex;
    
    // Cache de datasets
    std::vector<DatasetHandle> m_datasetCache;
    size_t m_datasetCacheSize;
    
    // Pool de buffers
    std::unordered_map<void*, WorkBuffer> m_activeBuffers;
    std::vector<void*> m_availableBuffers;
    
    // Estadísticas
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    std::atomic<size_t> m_totalMemory{0};
};

#endif // SMART_CACHE_H