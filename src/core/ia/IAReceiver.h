#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <chrono>
#include <vector>

// Forward declaration para la librería de red
namespace cpr { class Session; }

// Forward declaration para la estructura de trabajo de minería
struct MiningJob;

/**
 * @class IAReceiver
 * @brief Singleton que gestiona la comunicación con un servidor de IA externo.
 * * Se encarga de solicitar nonces predictivos y verificar los resultados
 * de forma segura y eficiente.
 */
class IAReceiver {
public:
    struct Config {
        bool enabled = false;
        std::string serverUrl = "http://localhost:8080";
        std::string apiKey;
        int timeoutMs = 5000;
    };

    // Obtiene la instancia única del Singleton
    static IAReceiver& getInstance();

    // Prohibir copias para garantizar que sea un Singleton
    IAReceiver(const IAReceiver&) = delete;
    IAReceiver& operator=(const IAReceiver&) = delete;

    // Configuración del servicio
    void configure(const Config& config);
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Operaciones principales con el servidor de IA
    std::vector<uint64_t> requestNonces(const MiningJob& job);
    bool verifyNonce(const std::string& nonce, const std::string& hash);

    // Métricas y utilidades
    std::pair<uint64_t, uint64_t> getStats() const;
    void resetStats();

private:
    IAReceiver();
    ~IAReceiver();

    std::unique_ptr<cpr::Session> m_session;
    mutable std::mutex m_mutex;
    
    Config m_config;
    bool m_enabled;
    
    std::chrono::steady_clock::time_point m_lastRequest;
    uint64_t m_requestCount;
    uint64_t m_successCount;
};