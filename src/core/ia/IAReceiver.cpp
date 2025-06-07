#include "core/ia/IAReceiver.h"
#include "utils/Logger.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <chrono>

using json = nlohmann::json;
using namespace std::chrono;

namespace {
    constexpr int MAX_RETRIES = 3;
    constexpr auto RETRY_DELAY = milliseconds(1000);
    constexpr auto REQUEST_TIMEOUT = milliseconds(5000);
    
    // Endpoints
    const std::string BASE_URL = "http://localhost:8080";
    const std::string NONCE_ENDPOINT = "/api/v1/nonce";
    const std::string VERIFY_ENDPOINT = "/api/v1/verify";
}

IAReceiver& IAReceiver::getInstance() {
    static IAReceiver instance;
    return instance;
}

IAReceiver::IAReceiver() 
    : m_session(std::make_unique<cpr::Session>())
    , m_enabled(false)
    , m_lastRequest(steady_clock::now())
    , m_requestCount(0)
    , m_successCount(0) {
    
    // Configurar sesión HTTP
    m_session->SetUrl(cpr::Url{BASE_URL});
    m_session->SetTimeout(REQUEST_TIMEOUT);
    m_session->SetVerifySsl(false); // Para desarrollo local
}

void IAReceiver::configure(const Config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_enabled = config.enabled;
    m_session->SetUrl(cpr::Url{config.serverUrl});
    m_session->SetTimeout(cpr::Timeout{config.timeoutMs});
    
    if (!config.apiKey.empty()) {
        m_session->SetHeader(cpr::Header{{"X-API-Key", config.apiKey}});
    }
    
    Logger::info("IAReceiver", "Configurado: enabled={}, url={}", 
                m_enabled, config.serverUrl);
}

std::optional<uint64_t> IAReceiver::requestNonce() {
    if (!m_enabled) return std::nullopt;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // Verificar rate limiting
        auto now = steady_clock::now();
        if (now - m_lastRequest < milliseconds(100)) {
            return std::nullopt;
        }
        m_lastRequest = now;
        m_requestCount++;

        // Preparar payload
        json payload = {
            {"timestamp", duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count()},
            {"request_id", m_requestCount}
        };

        // Realizar solicitud POST
        auto response = m_session->Post(
            cpr::Url{m_session->GetUrl() + NONCE_ENDPOINT},
            cpr::Body{payload.dump()},
            cpr::Header{{"Content-Type", "application/json"}}
        );

        // Verificar respuesta
        if (response.status_code == 200) {
            auto responseJson = json::parse(response.text);
            if (responseJson.contains("nonce")) {
                uint64_t nonce = responseJson["nonce"].get<uint64_t>();
                m_successCount++;
                Logger::debug("IAReceiver", "Nonce recibido: {}", nonce);
                return nonce;
            }
        }
        else {
            Logger::warn("IAReceiver", "Error en solicitud: {} - {}", 
                        response.status_code, response.text);
        }
    }
    catch (const std::exception& e) {
        Logger::error("IAReceiver", "Error en requestNonce: {}", e.what());
    }

    return std::nullopt;
}

bool IAReceiver::verifyNonce(uint64_t nonce, const std::string& hash) {
    if (!m_enabled) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // Preparar payload
        json payload = {
            {"nonce", nonce},
            {"hash", hash},
            {"timestamp", duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count()}
        };

        // Realizar solicitud POST
        auto response = m_session->Post(
            cpr::Url{m_session->GetUrl() + VERIFY_ENDPOINT},
            cpr::Body{payload.dump()},
            cpr::Header{{"Content-Type", "application/json"}}
        );

        // Verificar respuesta
        if (response.status_code == 200) {
            auto responseJson = json::parse(response.text);
            if (responseJson.contains("valid")) {
                return responseJson["valid"].get<bool>();
            }
        }
        
        Logger::warn("IAReceiver", "Error en verificación: {} - {}", 
                    response.status_code, response.text);
    }
    catch (const std::exception& e) {
        Logger::error("IAReceiver", "Error en verifyNonce: {}", e.what());
    }

    return false;
}

std::pair<uint64_t, uint64_t> IAReceiver::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return {m_requestCount, m_successCount};
}

void IAReceiver::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestCount = 0;
    m_successCount = 0;
    m_lastRequest = steady_clock::now();
}

bool IAReceiver::isEnabled() const {
    return m_enabled;
}

// Constructor privado y destructor
IAReceiver::~IAReceiver() = default;