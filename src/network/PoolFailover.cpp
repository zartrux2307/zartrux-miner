#include "PoolFailover.h"
#include "StratumClient.h" // Se incluye el .h completo solo aquí
#include "utils/Logger.h"
#include <thread>
#include <chrono>

PoolFailover::PoolFailover() {
    // La inicialización del cliente se mueve a start() para permitir re-conexiones.
}

PoolFailover::~PoolFailover() {
    stop();
}

void PoolFailover::setPools(const std::vector<PoolConfig>& pools) {
    m_pools = pools;
    m_currentIndex = 0;
}

void PoolFailover::start() {
    if (m_pools.empty()) {
        if (onConnectionError) {
            onConnectionError("La lista de pools está vacía.");
        }
        return;
    }
    tryNextPool();
}

void PoolFailover::stop() {
    if (m_client) {
        m_client->disconnect();
    }
}

void PoolFailover::tryNextPool() {
    if (m_pools.empty()) return;

    // Obtener el pool actual
    const auto& currentPool = m_pools[m_currentIndex];

    Logger::info("PoolFailover", "Intentando conectar al pool: %s:%d", currentPool.host.c_str(), currentPool.port);

    // Crear y configurar un nuevo cliente para cada intento de conexión principal
    m_client = std::make_unique<StratumClient>();

    // --- CORRECCIÓN: Se conectan los callbacks de C++ ---
    m_client->onConnected = [this]() { this->onConnected(); };
    m_client->onError = [this](const std::string& err) { this->onError(err); };
    m_client->onDisconnected = [this]() { this->onConnectionLost(); };
    
    m_client->connectToPool(currentPool.host, currentPool.port);
}

void PoolFailover::onConnected() {
    const auto& pool = m_pools[m_currentIndex];
    Logger::info("PoolFailover", "Conectado exitosamente al pool: %s:%d", pool.host.c_str(), pool.port);
    
    // Reseteamos los reintentos del pool actual en caso de éxito
    m_pools[m_currentIndex].retries = 0;

    if (onFailoverOccurred) {
        onFailoverOccurred(pool.host, pool.port);
    }
}

void PoolFailover::onError(const std::string& error) {
    Logger::warn("PoolFailover", "Error en el pool actual: %s", error.c_str());
    scheduleRetry();
}

void PoolFailover::onConnectionLost() {
    Logger::warn("PoolFailover", "Conexión perdida con el pool actual. Intentando con el siguiente...");
    scheduleRetry();
}

void PoolFailover::scheduleRetry() {
    if (m_pools.empty()) return;

    // Incrementar reintentos para el pool actual
    m_pools[m_currentIndex].retries++;

    if (m_pools[m_currentIndex].retries >= m_maxRetriesPerPool) {
        // Si se alcanzan los reintentos máximos, pasar al siguiente pool
        Logger::warn("PoolFailover", "Máximo de reintentos alcanzado para el pool actual. Cambiando al siguiente.");
        m_pools[m_currentIndex].retries = 0; // Resetear para futura rotación
        m_currentIndex = (m_currentIndex + 1) % m_pools.size();

        // Si hemos dado una vuelta completa y estamos de nuevo en el primer pool
        if (m_currentIndex == 0) {
            Logger::error("PoolFailover", "Todos los pools han fallado. Esperando antes de reintentar el ciclo.");
            if (onConnectionError) {
                onConnectionError("Todos los pools han fallado.");
            }
            // Esperar un tiempo antes de volver a empezar el ciclo para no saturar
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    } else {
        Logger::info("PoolFailover", "Reintentando conexión con el mismo pool (%d/%d)...", 
                     m_pools[m_currentIndex].retries, m_maxRetriesPerPool);
    }

    // Intentar la siguiente conexión
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Pequeña espera antes de reintentar
    tryNextPool();
}