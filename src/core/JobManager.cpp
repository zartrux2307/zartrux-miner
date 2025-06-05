#include "JobManager.h"
#include "utils/Logger.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include <csignal>
#elif _WIN32
#include <windows.h>
#endif

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

// ---- Apagado ordenado (para pruebas y backend) ----
void JobManager::shutdown() {
    m_shutdown = true;
    if (m_iaFetchThread.joinable()) {
        m_iaFetchThread.join();
    }
    saveCheckpoint();
}

// ---- Checkpoint y recuperación robusta ----
void JobManager::saveCheckpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream out(m_checkpointFile, std::ios::binary | std::ios::trunc);
    if (!out) return;
    size_t cpuq = m_cpuQueue.size();
    size_t iaq = m_iaQueue.size();
    out.write(reinterpret_cast<const char*>(&cpuq), sizeof(cpuq));
    for (const auto& n : m_cpuQueue)
        out.write(reinterpret_cast<const char*>(&n), sizeof(AnnotatedNonce));
    out.write(reinterpret_cast<const char*>(&iaq), sizeof(iaq));
    for (const auto& n : m_iaQueue)
        out.write(reinterpret_cast<const char*>(&n), sizeof(AnnotatedNonce));
    out.close();
    StatusExporter::exportStatusJSON(cpuq, iaq, m_validNonces, m_processedCount); // Backend
}

void JobManager::loadCheckpoint() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ifstream in(m_checkpointFile, std::ios::binary);
    if (!in) return;
    size_t cpuq = 0, iaq = 0;
    in.read(reinterpret_cast<char*>(&cpuq), sizeof(cpuq));
    m_cpuQueue.clear();
    for (size_t i = 0; i < cpuq; ++i) {
        AnnotatedNonce n;
        in.read(reinterpret_cast<char*>(&n), sizeof(AnnotatedNonce));
        m_cpuQueue.push_back(n);
    }
    in.read(reinterpret_cast<char*>(&iaq), sizeof(iaq));
    m_iaQueue.clear();
    for (size_t i = 0; i < iaq; ++i) {
        AnnotatedNonce n;
        in.read(reinterpret_cast<char*>(&n), sizeof(AnnotatedNonce));
        m_iaQueue.push_back(n);
    }
    in.close();
    Logger::info("Recuperado checkpoint: {} CPU, {} IA", cpuq, iaq);
}

// ---- Contribución IA (protección atomic) ----
void JobManager::setAIContribution(float ratio) {
    if (ratio < 0.0f || ratio > 1.0f)
        throw std::invalid_argument("Ratio IA debe estar entre 0.0 y 1.0");
    m_aiContribution.store(ratio, std::memory_order_release);
}
float JobManager::getAIContribution() const {
    return m_aiContribution.load(std::memory_order_acquire);
}

// ---- Endpoint IA ----
void JobManager::setIAEndpoint(const std::string& endpoint) {
    m_iaEndpoint = endpoint;
}
std::string JobManager::getIAEndpoint() const {
    return m_iaEndpoint;
}

// ---- Inyección IA: control flood, protección ----
void JobManager::injectIANonces(std::vector<AnnotatedNonce>&& nonces) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_iaQueue.size() > MAX_IA_QUEUE) {
        Logger::warn("Flood control: IA queue saturada, se descarta lote.");
        return;
    }
    std::sort(nonces.begin(), nonces.end(),
              [](const auto& a, const auto& b) { return a.confidence > b.confidence; });
    for (auto& n : nonces)
        m_iaQueue.push_back(std::move(n));
    m_cv.notify_all();
}

// ---- Afinidad de hilos por worker ----
void JobManager::setWorkerAffinity(size_t workerId, int cpuCore) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuCore, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
        Logger::warn("No se pudo asignar afinidad al worker {} (núcleo {})", workerId, cpuCore);
#endif
}

// ---- Batch de trabajo para cada worker ----
std::vector<AnnotatedNonce> JobManager::getWorkBatch(size_t workerId, size_t maxNonces) {
    PROFILE_FUNCTION();
    std::vector<AnnotatedNonce> batch;
    batch.reserve(maxNonces);

    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this, maxNonces] {
        return (m_cpuQueue.size() + m_iaQueue.size()) >= maxNonces || m_shutdown;
    });
    if (m_shutdown) return {};

    // Flood/sobrecarga protección
    if (floodControlActive(m_cpuQueue.size(), m_iaQueue.size()))
        return {};

    distributeBatch(workerId, batch, maxNonces);

    // Trigger backend Prometheus
    PrometheusExporter::exportGauge("jobmanager_cpu_queue", m_cpuQueue.size());
    PrometheusExporter::exportGauge("jobmanager_ia_queue", m_iaQueue.size());
    PrometheusExporter::exportCounter("jobmanager_batches", 1);

    // Reponer CPU pool si baja
    if (m_cpuQueue.size() < 50000)
        std::thread([this] { generateNonces(100000); }).detach();

    return batch;
}

// ---- Distribución inteligente con ratio IA/CPU ----
void JobManager::distributeBatch(size_t /*workerId*/, std::vector<AnnotatedNonce>& batch, size_t maxNonces) {
    const float aiRatio = m_aiContribution.load(std::memory_order_acquire);
    const size_t iaCount = static_cast<size_t>(aiRatio * maxNonces);
    const size_t cpuCount = maxNonces - iaCount;
    size_t iaTaken = std::min(iaCount, m_iaQueue.size());
    size_t cpuTaken = std::min(cpuCount, m_cpuQueue.size());

    for (size_t i = 0; i < iaTaken; ++i) {
        batch.push_back(std::move(m_iaQueue.front()));
        m_iaQueue.pop_front();
    }
    for (size_t i = 0; i < cpuTaken; ++i) {
        batch.push_back(std::move(m_cpuQueue.front()));
        m_cpuQueue.pop_front();
    }

    m_iaContributed += iaTaken;
    m_processedCount += batch.size();
}

// ---- Generación de nonces CPU ----
void JobManager::generateNonces(size_t count) {
    auto& cache = SmartCache::getInstance();
    auto baseNonce = cache.allocateNonceRange(count);

    std::vector<AnnotatedNonce> newNonces;
    newNonces.reserve(count);

    for (uint64_t i = 0; i < count; ++i) {
        newNonces.push_back({
            .value = baseNonce + i,
            .confidence = 1.0f, // Generado por CPU
            .timestamp = Profiler::getTimestamp()
        });
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& n : newNonces)
            m_cpuQueue.push_back(std::move(n));
    }
    m_cv.notify_all();
}

// ---- Control de flooding ----
bool JobManager::floodControlActive(size_t queueCpu, size_t queueIa) const {
    return queueCpu > FLOOD_CPU_THRESHOLD || queueIa > FLOOD_IA_THRESHOLD;
}

// ---- Fetch IA (webhook, retries) ----
std::vector<AnnotatedNonce> JobManager::fetchNoncesFromIA() {
    std::vector<AnnotatedNonce> nonces;
    for (int retries = 0; retries < MAX_RETRIES; ++retries) {
        try {
            auto response = cpr::Get(cpr::Url{m_iaEndpoint}, cpr::Timeout{3000});
            if (response.status_code == 200) {
                json parsed = json::parse(response.text);
                for (const auto& item : parsed) {
                    nonces.push_back({
                        .value = std::stoull(item["nonce"].get<std::string>()),
                        .confidence = item["confidence"].get<float>(),
                        .timestamp = Profiler::getTimestamp()
                    });
                }
                Logger::info("Obtenidos {} nonces desde IA", nonces.size());
                return nonces;
            }
        } catch (...) {
            Logger::error("Error al obtener nonces desde IA");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    return {};
}

void JobManager::fetchIANoncesBackground() {
    auto nonces = fetchNoncesFromIA();
    if (!nonces.empty()) {
        injectIANonces(std::move(nonces));
    }
}

// ---- Reporte de nonces procesados (con rotación de logs y callback backend) ----
void JobManager::reportProcessedNonces(const std::vector<std::pair<uint64_t, bool>>& results) {
    uint64_t validCount = 0;
    std::vector<std::pair<uint64_t, bool>> iaFeedback;

    for (const auto& [nonce, isValid] : results) {
        if (isValid) {
            validCount++;
            // El hash real debería obtenerse del proceso de minería
            submitValidNonce(nonce, "HASH_COMPUTADO");
        }
        iaFeedback.push_back({nonce, isValid});
    }
    m_validNonces += validCount;
    m_validNoncesSinceLog += validCount;

    if (validCount > 0) {
        IAReceiver::notifyValidationResults(iaFeedback);
    }

    // Rotar logs cada N nonces válidos
    if (m_validNoncesSinceLog >= LOG_ROTATE_EVERY) {
        std::rename("logs/nonces_exitosos.txt", ("logs/nonces_exitosos_" + std::to_string(time(nullptr)) + ".txt").c_str());
        m_validNoncesSinceLog = 0;
    }
    // Callback Prometheus/Backend
    PrometheusExporter::exportCounter("jobmanager_valid_nonces", validCount);
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
        Logger::error("Error al registrar nonce exitoso");
    }
}

// ---- Métricas de cola y contadores ----
size_t JobManager::getQueueSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cpuQueue.size() + m_iaQueue.size();
}

size_t JobManager::getProcessedCount() const {
    return m_processedCount.load();
}
