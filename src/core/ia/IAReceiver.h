#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <chrono>

namespace cpr { class Session; }

class IAReceiver {
public:
    struct Config {
        bool enabled = false;
        std::string serverUrl = "http://localhost:8080";
        std::string apiKey;
        int timeoutMs = 5000;
    };

    // Singleton
    static IAReceiver& getInstance();

    // No permitir copias
    IAReceiver(const IAReceiver&) = delete;
    IAReceiver& operator=(const IAReceiver&) = delete;

    // Configuración
    void configure(const Config& config);

    // Operaciones principales
    std::optional<uint64_t> requestNonce();
    bool verifyNonce(uint64_t nonce, const std::string& hash);

    // Utilidades
    std::pair<uint64_t, uint64_t> getStats() const;
    void reset();
    bool isEnabled() const;

private:
    IAReceiver();
    ~IAReceiver();

    std::unique_ptr<cpr::Session> m_session;
    mutable std::mutex m_mutex;
    bool m_enabled;
    std::chrono::steady_clock::time_point m_lastRequest;
    uint64_t m_requestCount;
    uint64_t m_successCount;
};