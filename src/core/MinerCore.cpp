#include "MinerCore.h"
#include "utils/Logger.h"
#include "metrics/PrometheusExporter.h"
#include "core/SystemMonitor.h"
#include "utils/StatusExporter.h"
#include "zarbackend/WebsocketBackend.h"  // Suponiendo módulo WebSocket para zarbackend
#include <fstream>
#include <sstream>
#include <csignal>
#include <thread>
#include <iomanip>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

using namespace std::chrono;

namespace {
    const std::string CHECKPOINT_FILE = "miner_checkpoint.json";
}

MinerCore::MinerCore(std::shared_ptr<JobManager> jobManager, unsigned threadCount)
    : m_jobManager(std::move(jobManager)),
      m_numThreads(threadCount ? threadCount : std::thread::hardware_concurrency()),
      m_mining(false),
      m_acceptedShares(0) 
{
    if (m_numThreads == 0) m_numThreads = 4;
    Logger::info("[MinerCore] Configurado con {} hilos", m_numThreads);

    // Intentar restaurar estado si es posible
    loadCheckpoint();
}

MinerCore::~MinerCore() {
    stopMining();
    cleanupWorkers();
    cleanupRandomX();
    saveCheckpoint();
}

void MinerCore::cleanupRandomX() {
    for (auto vm : m_workerVMs) {
        if (vm) randomx_destroy_vm(vm);
    }
    m_workerVMs.clear();
    if (m_rxCache) {
        randomx_release_cache(m_rxCache);
        m_rxCache = nullptr;
    }
}

bool MinerCore::initialize(const MiningConfig& config) {
    stopMining();
    cleanupWorkers();
    cleanupRandomX();
    m_config = config;

    try {
        if (config.seed) {
            Logger::info("[MinerCore] Inicializando RandomX con semilla: {}", config.seed.value());
            m_rxCache = randomx_alloc_cache(RANDOMX_FLAG_DEFAULT);
            if (!m_rxCache) {
                Logger::error("[MinerCore] Error al asignar cache de RandomX");
                return false;
            }
            randomx_init_cache(m_rxCache, config.seed.value().data(), config.seed.value().size());
            for (unsigned i = 0; i < m_numThreads; ++i) {
                randomx_vm* vm = randomx_create_vm(RANDOMX_FLAG_DEFAULT, m_rxCache, nullptr);
                if (!vm) {
                    Logger::error("[MinerCore] Error al crear VM para hilo {}", i);
                    cleanupRandomX();
                    return false;
                }
                m_workerVMs.push_back(vm);
            }
        } else {
            Logger::warn("[MinerCore] Advertencia: No se proporcionó semilla para RandomX");
        }
        for (unsigned i = 0; i < m_numThreads; ++i) {
            WorkerThread::Config cfg{
                i,
                m_workerVMs[i],
                m_config.noncePosition,
                m_config.nonceSize,
                m_config.nonceEndianness
            };
            auto worker = std::make_unique<WorkerThread>(i, *m_jobManager, cfg);
            m_workers.emplace_back(std::move(worker));
        }
        Logger::info("[MinerCore] Inicialización completa con {} hilos. Modo: {}", m_numThreads, m_config.mode);
        broadcastEvent("init", "Miner inicializado");
        return true;
    }
    catch (const std::exception& ex) {
        Logger::error("[MinerCore] Excepción durante la inicialización: {}", ex.what());
        cleanupRandomX();
        return false;
    }
}

void MinerCore::startMining() {
    try {
        if (m_mining.exchange(true)) {
            Logger::warn("[MinerCore] La minería ya está activa.");
            return;
        }
        m_miningStartTime = steady_clock::now();
        m_acceptedShares = 0;

        for (unsigned i = 0; i < m_workers.size(); ++i) {
            setAffinity(i);
            m_workers[i]->start();
        }
        Logger::info("[MinerCore] Minería iniciada en modo: {}", m_config.mode);
        broadcastEvent("start", "Minería iniciada");
    } catch (const std::exception& ex) {
        Logger::error("[MinerCore] Error en startMining: {}", ex.what());
        broadcastEvent("error", ex.what());
    }
}

void MinerCore::stopMining() {
    try {
        if (!m_mining.exchange(false)) {
            Logger::warn("[MinerCore] La minería ya estaba detenida.");
            return;
        }
        for (auto& worker : m_workers) worker->stop();
        for (auto& worker : m_workers)
            if (worker->joinable()) worker->join();
        Logger::info("[MinerCore] Minería detenida. Tiempo activa: {} segundos", getMiningTime());
        broadcastEvent("stop", "Minería detenida");
    } catch (const std::exception& ex) {
        Logger::error("[MinerCore] Error en stopMining: {}", ex.what());
        broadcastEvent("error", ex.what());
    }
    saveCheckpoint();
}

long MinerCore::getMiningTime() const {
    if (!m_mining) return 0;
    auto duration = duration_cast<seconds>(steady_clock::now() - m_miningStartTime.load());
    return duration.count();
}

int MinerCore::getActiveThreads() const {
    int count = 0;
    for (const auto& worker : m_workers) if (worker->isRunning()) count++;
    return count;
}

int MinerCore::getAcceptedShares() const { return m_acceptedShares.load(); }

float MinerCore::getCurrentDifficulty() const { return m_jobManager->getCurrentDifficulty(); }
std::string MinerCore::getCurrentBlock() const { return std::to_string(m_jobManager->getCurrentBlockHeight()); }
std::string MinerCore::getBlockStatus() const { return m_jobManager->isBlockValidating() ? "Validando" : "Minando"; }
float MinerCore::getTemperature() const { return SystemMonitor::instance().getCPUTemperature(); }
std::string MinerCore::getTempStatus() const {
    float temp = getTemperature();
    if (temp < 50) return "Normal";
    if (temp < 70) return "Caliente";
    return "Muy Caliente";
}

void MinerCore::cleanupWorkers() {
    for (auto& worker : m_workers) {
        if (worker) {
            worker->stop();
            if (worker->joinable()) worker->join();
        }
    }
    m_workers.clear();
    Logger::info("[MinerCore] Hilos limpiados correctamente.");
}

void MinerCore::setNumThreads(unsigned count) {
    if (m_mining) {
        Logger::warn("[MinerCore] No se puede modificar la cantidad de hilos mientras se mina.");
        return;
    }
    m_numThreads = count > 0 ? count : std::thread::hardware_concurrency();
    Logger::info("[MinerCore] Número de hilos actualizado a {}", m_numThreads);
}

std::vector<MinerCore::WorkerStats> MinerCore::getWorkerStats() const {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    std::vector<WorkerStats> stats;
    stats.reserve(m_workers.size());
    for (const auto& worker : m_workers) {
        WorkerStats s;
        s.totalHashes = worker->getHashesProcessed();
        s.acceptedHashes = worker->getAcceptedHashes();
        s.iaNoncesUsed = worker->getMetrics().iaNoncesUsed.load();
        s.hashRate = worker->getMetrics().hashRate.load();
        stats.push_back(s);
    }
    return stats;
}

void MinerCore::updateMetrics() {
    auto stats = getWorkerStats();
    uint64_t totalHashes = 0, acceptedHashes = 0, iaNoncesUsed = 0;
    double totalHashRate = 0.0;
    for (const auto& s : stats) {
        totalHashes += s.totalHashes;
        acceptedHashes += s.acceptedHashes;
        iaNoncesUsed += s.iaNoncesUsed;
        totalHashRate += s.hashRate;
    }
    PrometheusExporter::instance().record({
        {"total_hashes", totalHashes},
        {"accepted_hashes", acceptedHashes},
        {"ia_nonces_used", iaNoncesUsed},
        {"total_hash_rate", static_cast<uint64_t>(totalHashRate)},
        {"active_threads", static_cast<uint64_t>(getActiveThreads())}
    });
    StatusExporter::instance().exportJSON({
        {"hashrate", totalHashRate},
        {"threads", m_numThreads},
        {"shares", acceptedHashes},
        {"temperature", getTemperature()},
        {"uptime", getMiningTime()}
    });
    broadcastEvent("metrics", std::to_string(totalHashRate));
    Logger::debug("[MinerCore] Métricas actualizadas: Total hashes={}, Aceptados={}, IA={}",
                 totalHashes, acceptedHashes, iaNoncesUsed);
}

void MinerCore::restartWorker(unsigned id) {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    if (id >= m_workers.size()) return;
    m_workers[id]->stop();
    if (m_workers[id]->joinable()) m_workers[id]->join();
    WorkerThread::Config cfg{
        id,
        m_workerVMs[id],
        m_config.noncePosition,
        m_config.nonceSize,
        m_config.nonceEndianness
    };
    m_workers[id] = std::make_unique<WorkerThread>(id, *m_jobManager, cfg);
    setAffinity(id);
    m_workers[id]->start();
    Logger::info("[MinerCore] Hilo {} reiniciado", id);
}

void MinerCore::setAffinity(unsigned threadId) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadId % std::thread::hardware_concurrency(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

void MinerCore::saveCheckpoint() const {
    try {
        std::ofstream out(CHECKPOINT_FILE);
        if (!out) return;
        out << "{\n";
        out << "  \"lastBlockHeight\": " << m_jobManager->getCurrentBlockHeight() << ",\n";
        out << "  \"totalHashes\": " << getWorkerStats().front().totalHashes << ",\n";
        out << "  \"acceptedShares\": " << m_acceptedShares.load() << ",\n";
        out << "  \"miningStart\": \"" << std::chrono::duration_cast<std::chrono::seconds>(
            m_miningStartTime.load().time_since_epoch()).count() << "\"\n";
        out << "}\n";
    } catch (...) {
        Logger::warn("[MinerCore] Error guardando checkpoint (ignorado).");
    }
}

bool MinerCore::loadCheckpoint() {
    try {
        std::ifstream in(CHECKPOINT_FILE);
        if (!in) return false;
        std::string line;
        while (std::getline(in, line)) {
            // Implementar carga real según formato (puede usarse nlohmann::json o similar)
        }
        Logger::info("[MinerCore] Checkpoint restaurado correctamente.");
        return true;
    } catch (...) {
        Logger::warn("[MinerCore] Error restaurando checkpoint (ignorado).");
        return false;
    }
}

void MinerCore::broadcastEvent(const std::string& eventType, const std::string& payload) const {
    // Hook para Prometheus/web/zarbackend (WebSocket o HTTP)
    WebsocketBackend::instance().broadcast(eventType, payload);
}
