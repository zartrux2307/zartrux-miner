#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <cstdint>

// --- CORRECCIÓN: Se usa una declaración adelantada para evitar inclusiones circulares ---
class StratumClient;

/**
 * @class PoolFailover
 * @brief Gestiona una lista de pools y cambia automáticamente al siguiente
 * en caso de fallo de conexión (failover).
 */
class PoolFailover {
public:
    struct PoolConfig {
        std::string host;
        uint16_t port;
        int retries{0}; // Reintentos actuales para este pool
    };

    // --- CORRECCIÓN: Se reemplazan signals de Qt por callbacks de C++ estándar ---
    std::function<void(const std::string& host, uint16_t port)> onFailoverOccurred;
    std::function<void(const std::string& error)> onConnectionError;

    explicit PoolFailover();
    ~PoolFailover();

    void setPools(const std::vector<PoolConfig>& pools);
    void start();
    void stop();

private:
    void tryNextPool();
    void onConnected();
    void onError(const std::string& error);
    void onConnectionLost();
    void scheduleRetry();

    std::vector<PoolConfig> m_pools;
    size_t m_currentIndex = 0;
    int m_maxRetriesPerPool = 3;
    
    // Se usa un puntero único para gestionar el ciclo de vida del cliente Stratum
    std::unique_ptr<StratumClient> m_client;
};