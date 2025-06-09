#include "core/ia/IAReceiver.h"
#include "core/JobManager.h" // Se incluye para la definición de MiningJob
#include "utils/Logger.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;
using namespace std::chrono;

namespace {
    // Endpoints de la API
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
}

IAReceiver::~IAReceiver() = default;

void IAReceiver::configure(const Config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_enabled = config.enabled;

    // Configurar la sesión de CPR con los nuevos parámetros
    m_session->SetTimeout(milliseconds(m_config.timeoutMs));
    m_session->SetHeader({
        {"Content-Type", "application/json"},
        {"X-API-Key", m_config.apiKey}
    });
    Logger::info("IAReceiver", "Configuración del receptor de IA actualizada. URL: %s, Habilitado: %s", 
                 m_config.serverUrl.c_str(), m_enabled ? "Sí" : "No");
}

void IAReceiver::setEnabled(bool enabled) {
    m_enabled = enabled;
}

bool IAReceiver::isEnabled() const {
    return m_enabled;
}

std::vector<uint64_t> IAReceiver::requestNonces(const MiningJob& job) {
    if (!m_enabled) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastRequest = steady_clock::now();
    m_requestCount++;

    try {
        json payload = {
            {"job_id", job.id},
            {"blob", job.blob}
        };
        
        // --- CORRECCIÓN: Se construye la URL completa y se pasa a Post ---
        cpr::Response response = m_session->Post(
            cpr::Url{m_config.serverUrl + NONCE_ENDPOINT},
            cpr::Body{payload.dump()}
        );

        if (response.status_code == 200) {
            auto responseJson = json::parse(response.text);
            if (responseJson.contains("nonces")) {
                m_successCount++;
                return responseJson["nonces"].get<std::vector<uint64_t>>();
            }
        }
        
        Logger::warn("IAReceiver", "Error en la solicitud de nonces: Status %ld - %s", 
                     response.status_code, response.text.c_str());

    } catch (const std::exception& e) {
        Logger::error("IAReceiver", "Excepción en requestNonces: %s", e.what());
    }

    return {};
}

bool IAReceiver::verifyNonce(const std::string& nonce, const std::string& hash) {
    if (!m_enabled) {
        return false;
    }
    
    try {
        json payload = {
            {"nonce", nonce},
            {"hash", hash},
            {"timestamp", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()}
        };
        
        // --- CORRECCIÓN: Se construye la URL completa y se pasa a Post ---
        cpr::Response response = m_session->Post(
            cpr::Url{m_config.serverUrl + VERIFY_ENDPOINT},
            cpr::Body{payload.dump()}
        );

        if (response.status_code == 200) {
            auto responseJson = json::parse(response.text);
            if (responseJson.contains("valid")) {
                return responseJson["valid"].get<bool>();
            }
        }
        
        Logger::warn("IAReceiver", "Error en verificación de nonce: Status %ld - %s", 
                     response.status_code, response.text.c_str());

    } catch (const std::exception& e) {
        Logger::error("IAReceiver", "Excepción en verifyNonce: %s", e.what());
    }

    return false;
}

std::pair<uint64_t, uint64_t> IAReceiver::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return {m_requestCount, m_successCount};
}

void IAReceiver::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestCount = 0;
    m_successCount = 0;
}