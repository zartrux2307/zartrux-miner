#include "core/threads/WorkerThread.h"
#include "utils/Logger.h"
#include "core/ia/IAReceiver.h"
#include <randomx.h>
#include <fmt/format.h>
#include <sstream>
#include <iomanip>
#include <chrono>

using namespace std::chrono;

WorkerThread::WorkerThread(unsigned id, JobManager& jobManager, const Config& config)
    : m_id(id)
    , m_jobManager(jobManager)
    , m_config(config)
    , m_running(false)
{
    Logger::info("WorkerThread", "Hilo {} creado", m_id);
}

WorkerThread::~WorkerThread() {
    stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void WorkerThread::start() {
    if (m_running.exchange(true)) {
        return; // Ya está corriendo
    }
    m_thread = std::thread(&WorkerThread::run, this);
    if (m_config.cpuAffinity >= 0) {
        setCPUAffinity(m_config.cpuAffinity);
    }
    Logger::debug("WorkerThread", "Hilo {} iniciado", m_id);
}

void WorkerThread::stop() {
    m_running.store(false);
}

void WorkerThread::restart() {
    stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    Logger::debug("WorkerThread", "Reiniciando hilo {}", m_id);
    start();
}

bool WorkerThread::setCPUAffinity(int core) {
#ifdef _WIN32
    HANDLE thread = m_thread.native_handle();
    DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << core);
    if (!SetThreadAffinityMask(thread, mask)) {
        Logger::error("WorkerThread", "Error al establecer afinidad de CPU para hilo {}", m_id);
        return false;
    }
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int rc = pthread_setaffinity_np(m_thread.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        Logger::error("WorkerThread", "Error al establecer afinidad de CPU para hilo {}", m_id);
        return false;
    }
#endif
    return true;
}

void WorkerThread::run() {
    try {
        auto& iaReceiver = IAReceiver::getInstance();
        uint64_t hashCount = 0;
        auto lastHashTime = steady_clock::now();
        std::vector<uint8_t> data;
        NonceValidator validator;

        while (m_running) {
            // Obtener trabajo actual
            auto job = m_jobManager.getCurrentJob();
            if (!job) {
                std::this_thread::sleep_for(milliseconds(100));
                continue;
            }

            // Preparar datos para hash
            data = job->getData();
            uint64_t nonce;

            // Modo híbrido: alternar entre IA y CPU
            if (m_hybridToggle.load()) {
                // Intentar obtener nonce de IA
                auto iaNonce = iaReceiver.requestNonce();
                if (iaNonce) {
                    nonce = *iaNonce;
                    m_metrics.iaNoncesUsed++;
                } else {
                    // Si no hay nonce de IA, generar uno
                    nonce = m_jobManager.generateNonce();
                }
                m_hybridToggle.store(false);
            } else {
                nonce = m_jobManager.generateNonce();
                m_hybridToggle.store(true);
            }

            // Insertar nonce en datos
            validator.insertNonce(data, nonce, m_config.noncePosition, 
                               m_config.nonceSize, 
                               m_config.nonceEndianness ? NonceValidator::Endianness::BIG 
                                                      : NonceValidator::Endianness::LITTLE);

            // Calcular hash
            NonceValidator::hash_t hash;
            randomx_calculate_hash(static_cast<randomx_vm*>(m_config.vm),
                                 data.data(), data.size(), hash.data());

            // Verificar hash
            if (validator.isValidFast(hash, job->getDifficulty())) {
                std::string hashHex = toHexString(hash);
                m_jobManager.submitValidNonce(nonce, hashHex);
                m_metrics.acceptedHashes++;
            }

            // Actualizar métricas
            hashCount++;
            m_metrics.totalHashes++;

            // Calcular tasa de hash cada segundo
            auto now = steady_clock::now();
            auto elapsed = duration_cast<seconds>(now - lastHashTime).count();
            if (elapsed >= 1) {
                double hashRate = static_cast<double>(hashCount) / elapsed;
                m_metrics.hashRate.store(hashRate);
                hashCount = 0;
                lastHashTime = now;
                Logger::debug("WorkerThread", "Hilo {} - Hash rate: {:.2f} H/s", m_id, hashRate);
            }

            // Throttling si está configurado
            if (m_config.throttle < 1.0) {
                std::this_thread::sleep_for(microseconds(
                    static_cast<int64_t>((1.0 - m_config.throttle) * 1000)));
            }
        }
    }
    catch (const std::exception& ex) {
        m_metrics.hasCriticalError = true;
        Logger::error("WorkerThread", "Error en hilo {}: {}", m_id, ex.what());
    }
}

std::string WorkerThread::toHexString(const NonceValidator::hash_t& hash) const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : hash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}