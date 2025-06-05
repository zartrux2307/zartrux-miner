#include "WorkerThread.h"
#include "core/JobManager.h"
#include "core/NonceValidator.h"
#include "core/MiningModeManager.h"
#include "core/ia/IAReceiver.h"
#include "utils/Logger.h"

#include <randomx.h>
#include <random>
#include <chrono>
#include <thread>
#include <vector>
#include <exception>
#if defined(_WIN32)
    #include <windows.h>
#endif

// Solo para minería real de Monero/RandomX. No válido para ningún otro algoritmo ni para simulaciones.

class LocalNonceGenerator {
public:
    LocalNonceGenerator() :
        m_engine(std::random_device{}()),
        m_distribution(0, std::numeric_limits<uint64_t>::max()) {}

    uint64_t next() {
        return m_distribution(m_engine);
    }
private:
    std::mt19937_64 m_engine;
    std::uniform_int_distribution<uint64_t> m_distribution;
};

WorkerThread::WorkerThread(unsigned id, JobManager& jobManager, const Config& config)
    : m_id(id), m_jobManager(jobManager), m_config(config)
{
    Logger::debug("[WorkerThread {}] Inicializado con VM: {}", id, (void*)config.vm);
}

WorkerThread::~WorkerThread() {
    stop();
}

void WorkerThread::start() {
    if (!m_running.exchange(true)) {
        m_thread = std::thread(&WorkerThread::run, this);
    }
}

void WorkerThread::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool WorkerThread::isRunning() const {
    return m_running.load();
}

WorkerThread::Metrics WorkerThread::getMetrics() const {
    return m_metrics;
}

void WorkerThread::restart() {
    Logger::warn("[WorkerThread {}] Reiniciando tras excepción...", m_id);
    stop();
    start();
}

bool WorkerThread::setCPUAffinity(int cpuCore) {
#if defined(_WIN32)
    if (cpuCore < 0) return false;
    DWORD_PTR mask = (1ULL << cpuCore);
    HANDLE threadHandle = (HANDLE)m_thread.native_handle();
    DWORD_PTR result = SetThreadAffinityMask(threadHandle, mask);
    if (result == 0) {
        Logger::warn("[WorkerThread {}] No se pudo fijar afinidad CPU (Windows).", m_id);
        return false;
    } else {
        Logger::info("[WorkerThread {}] Afinidad fijada al core {} (Windows)", m_id, cpuCore);
        return true;
    }
#else
    return false; // Afinidad solo soportada en Windows por ahora
#endif
}

void WorkerThread::run() {
    LocalNonceGenerator localGenerator;
    auto& modeManager = MiningModeManager::getInstance();
    auto& iaReceiver = IAReceiver::getInstance();

    // Fijar afinidad de CPU si procede
    setCPUAffinity(m_config.cpuAffinity);

    Logger::info("[WorkerThread {}] Iniciado. Modo actual: {}", 
        m_id, modeToString(modeManager.getCurrentMode()));

    auto lastPerfUpdate = std::chrono::steady_clock::now();
    uint64_t lastHashesCount = 0;

    try {
        while (m_running) {
            // 1. Obtener trabajo actual
            auto job = m_jobManager.getCurrentJob();
            if (!job.valid) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // 2. Selección de modo y obtención de nonce
            uint64_t nonce = 0;
            bool isFromIA = false;
            const auto mode = modeManager.getCurrentMode();

            if (mode == MiningMode::IA) {
                nonce = iaReceiver.fetchNonce();
                isFromIA = true;
                m_metrics.iaNoncesUsed++;
            } else if (mode == MiningMode::HYBRID) {
                if (m_hybridToggle.exchange(!m_hybridToggle)) {
                    nonce = iaReceiver.fetchNonce();
                    isFromIA = true;
                    m_metrics.iaNoncesUsed++;
                } else {
                    nonce = localGenerator.next();
                }
            } else {
                nonce = localGenerator.next();
            }

            // 3. Insertar nonce en el blob
            std::vector<uint8_t> blob = job.blob;
            NonceValidator::insertNonce(blob, nonce, m_config.noncePosition, m_config.nonceSize, m_config.nonceEndianness);

            // 4. Calcular hash con RandomX
            NonceValidator::hash_t hash;
            randomx_calculate_hash(m_config.vm, blob.data(), blob.size(), hash.data());

            // 5. Validar hash vs target
            if (NonceValidator::isValidFast(hash, job.target)) {
                const auto hashStr = toHexString(hash);
                m_jobManager.submitValidNonce(std::to_string(nonce), hashStr);

                if (isFromIA) {
                    iaReceiver.reportSuccess(nonce, hashStr);
                }
                m_metrics.acceptedHashes++;
            }
            m_metrics.totalHashes++;

            // 6. Throttling (por PowerSafe/AdaptiveScheduler o config)
            if (m_config.throttle < 1.0) {
                std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(1000 * (1.0 - m_config.throttle))));
            }

            // 7. Actualizar métricas de rendimiento cada segundo
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPerfUpdate).count();

            if (elapsed > 1000) {
                const uint64_t hashesDelta = m_metrics.totalHashes - lastHashesCount;
                m_metrics.hashRate = (hashesDelta * 1000.0) / elapsed;

                lastPerfUpdate = now;
                lastHashesCount = m_metrics.totalHashes;
                Logger::debug("[WorkerThread {}] Hash rate: {:.2f} H/s", m_id, m_metrics.hashRate.load());
            }
        }
    } catch (const std::exception& ex) {
        Logger::error("[WorkerThread {}] Excepción crítica: {}", m_id, ex.what());
        restart(); // Reinicia el hilo tras excepción
    } catch (...) {
        Logger::error("[WorkerThread {}] Excepción crítica desconocida", m_id);
        restart();
    }

    Logger::info("[WorkerThread {}] Detenido", m_id);
}

std::string WorkerThread::toHexString(const NonceValidator::hash_t& hash) const {
    static constexpr char hexDigits[] = "0123456789abcdef";
    std::string output;
    output.reserve(64);
    for (uint8_t byte : hash) {
        output.push_back(hexDigits[byte >> 4]);
        output.push_back(hexDigits[byte & 0x0F]);
    }
    return output;
}
