#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <optional>

// HTTP
#include <cpr/cpr.h>
// JSON
#include <nlohmann/json.hpp>

enum class IAProtocol {
    HTTP,   // Polling REST
    ZMQ     // Push ZMQ
};

class IAReceiver {
public:
    struct Config {
        // --- HTTP
        std::string serverUrl = "http://127.0.0.1:5000";
        int requestTimeoutMs = 3000;
        int maxRetries = 3;
        // --- ZMQ
        std::string ip = "127.0.0.1";
        int port = 5557;
        std::string authToken;
        int recvTimeoutMs = 1000;
        // --- General
        bool verbose = false;
        IAProtocol protocol = IAProtocol::HTTP;
    };

    using NonceCallback = std::function<void(const std::vector<uint64_t>&)>;
    struct Stats {
        std::atomic<uint64_t> fetchAttempts{0}, fetchSuccesses{0}, reportAttempts{0}, reportSuccesses{0};
        std::atomic<uint64_t> zmqMessages{0}, zmqErrors{0}, zmqAuthFails{0}, zmqNoncesRecv{0}, zmqParseErr{0};
    };

    explicit IAReceiver(const Config& config = Config());
    ~IAReceiver();

    void start();      // Solo necesario en ZMQ/push (lanza hilo)
    void stop();       // Detiene listener ZMQ si está corriendo

    // Para modo HTTP (polling síncrono)
    std::vector<uint64_t> fetchNonces(int count = 10);
    void reportResult(uint64_t nonce, bool accepted, const std::string& hash = "");

    // Estadísticas
    Stats getStats() const;

    // ZMQ only (push, multihilo)
    void setNonceCallback(NonceCallback cb);

    bool isRunning() const { return m_running.load(); }
    // Singleton para integración rápida (opcional, recomendado solo para servicios)
    static IAReceiver& instance(const Config& config = Config());

private:
    Config m_config;
    mutable std::mutex m_mutex;

    // HTTP client
    cpr::Session m_httpSession;

    // ZMQ state
    void* m_zmqContext = nullptr;
    void* m_zmqSocket = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    NonceCallback m_callback;

    Stats m_stats;
    void cleanup();

    // ZMQ receive loop
    void zmqListenLoop();
    void handleJsonMessage(const std::string& msg);

    // Logging
    void logError(const std::string& msg) const;
    void logInfo(const std::string& msg) const;
};
