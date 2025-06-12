#include "core/JobManager.h"
#include "MiningModeManager.h"
#include "utils/Logger.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include <csignal>
#include <windows.h>

using json = nlohmann::json;

// ---- Singleton ----
JobManager& JobManager::getInstance() {
    static JobManager instance;
    return instance;
}

// ---- Constructor/Destructor: arranque fetch de IA y recuperación checkpoint ----
JobManager::JobManager()
    : m_iaEndpoint("http://127.0.0.1:4444") {
    loadCheckpoint();

    // Hilo fetch IA nonces
    m_iaFetchThread = std::thread([this] {
        while (!m_shutdown) {
            try {
                fetchIANoncesBackground();
                saveCheckpoint(); // checkpoint periódico
            } catch (const std::exception& ex) {
                Logger::error("Excepción hilo IA fetch: {}", ex.what());
            }
            std::this_thread::sleep_for(IA_FETCH_INTERVAL);
        }
    });
}

JobManager::~JobManager() {
    shutdown();
}
void JobManager::setNewJob(const MiningJob& newJob) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentJob = newJob;
    m_job_available = true;
    m_cv.notify_all(); // Notificar a todos los hilos que hay un nuevo trabajo
}

const std::vector<uint8_t>& JobManager::getCurrentBlob() const {
    // NOTA: Devolver directamente el blob. Se asume que el acceso es seguro
    // o gestionado por el ciclo de vida del JobManager.
    return m_currentJob.blob;
}

const std::array<uint8_t, 32>& JobManager::getCurrentTarget() const {
    return m_currentJob.targetBin;
}

bool JobManager::hasActiveJob() const {
    return m_job_available.load();
}

// ---- Apagado ordenado (para pruebas y backend) ----
void JobManager::shutdown() {
    m_shutdown = true;
    m_cv.notify_all(); // Despertar hilos en espera
    if (m_iaFetchThread.joinable()) {
        m_iaFetchThread.join();
    }
    saveCheckpoint();
}

// ---- Gestión de contribución IA ----
void JobManager::setAIContribution(float ratio) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aiContribution = std::clamp(ratio, 0.0f, 1.0f);
}

float JobManager::getAIContribution() const {
    return m_aiContribution.load();
}

// ---- Gestión de endpoints IA ----
void JobManager::setIAEndpoint(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_iaEndpoint = endpoint;
}

std::string JobManager::getIAEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_iaEndpoint;
}

// ---- Lógica de colas ----
void JobManager::injectIANonces(const std::vector<uint64_t>& nonces) {
    if (nonces.empty()) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint64_t nonce : nonces) {
        if (m_iaQueue.size() < MAX_QUEUE_SIZE) {
            m_iaQueue.push_back({nonce, 0.9f, (uint64_t)time(nullptr)});
            m_iaContributed++;
        }
    }
    m_cv.notify_one(); // Notificar que hay trabajo
}

void JobManager::processNonces(const std::vector<uint64_t>& nonces, const std::vector<bool>& results) {
    if (nonces.size() != results.size()) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    size_t validCount = 0;
    std::vector<std::pair<uint64_t, bool>> iaFeedback;

    for (size_t i = 0; i < nonces.size(); ++i) {
        m_processedCount++;
        bool isValid = results[i];








        
        uint64_t nonce = nonces[i];
        if (isValid) {
            validCount++;
            submitValidNonce(nonce, "HASH_COMPUTADO");
        }
        iaFeedback.push_back({nonce, isValid});
    }
    m_validNonces += validCount;
    m_validNoncesSinceLog += validCount;

    if (validCount > 0) {
        IAReceiver::getInstance().verifyNonce(std::to_string(iaFeedback[0].first), "HASH_VALIDADO");
    }

    // Rotar logs cada N nonces válidos
    if (m_validNoncesSinceLog >= LOG_ROTATE_EVERY) {
        std::rename("logs/nonces_exitosos.txt", ("logs/nonces_exitosos_" + std::to_string(time(nullptr)) + ".txt").c_str());
        m_validNoncesSinceLog = 0;
    }
    
    // --- CORRECCIÓN ---
    // Se elimina la llamada a PrometheusExporter. La llamada a StatusExporter ya existe y es la correcta.
    // PrometheusExporter::exportCounter("jobmanager_valid_nonces", validCount);
    StatusExporter::exportStatusJSON(m_cpuQueue.size(), m_iaQueue.size(), m_validNonces, m_processedCount);
}

// ---- Registro nonce exitoso ----
void JobManager::submitValidNonce(uint64_t nonce, const std::string& hash) {
    try {
        std::ofstream out("logs/nonces_exitosos.txt", std::ios::app);
        if (out) {
            out << nonce << "," << hash << "\n";
            Logger::info("Nonce válido registrado: {}", nonce);
        }
    } catch (...) {
        Logger::error("Fallo al registrar nonce válido en archivo.");
    }
}