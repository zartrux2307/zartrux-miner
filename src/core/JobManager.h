#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <thread>
#include <fstream>
#include <array>

#include "core/SmartCache.h"
#include "core/NonceValidator.h"
#include "core/ia/IAReceiver.h"
#include "runtime/Profiler.h"
#include "utils/StatusExporter.h"

// Estructura para almacenar la información de un trabajo de minería.
struct MiningJob {
    std::string id;
    std::string blob;
    std::string target;
    std::array<uint8_t, 32> targetBin; // Target convertido a binario para comparaciones rápidas
    uint64_t height = 0;
};

// Estructura para los nonces, anotados con la confianza de la IA.
struct AnnotatedNonce {
    uint64_t value;
    float confidence = 1.0f; // 1.0 para nonces de CPU, < 1.0 para nonces de IA
    uint64_t timestamp;
};

/**
 * @class JobManager
 * @brief Orquesta la distribución de trabajo a los hilos mineros,
 * gestionando las colas de nonces (CPU vs IA) y los resultados.
 */
class JobManager {
public:
    static JobManager& getInstance();

    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;

    // --- Interfaz Pública ---

    // Configuración
    void setAIContribution(float ratio);
    float getAIContribution() const;

    // Gestión de Trabajos
    void setNewJob(const MiningJob& newJob);
    std::vector<AnnotatedNonce> getWorkBatch(size_t workerId, size_t maxNonces);
    
    // Inyección y Procesamiento de Nonces
    void injectIANonces(const std::vector<uint64_t>& nonces);
    void submitValidNonce(uint64_t nonce, const std::string& hash);

    // Obtención de datos del trabajo actual (para los hilos)
    std::vector<uint8_t> getCurrentBlob() const;
    const std::array<uint8_t, 32>& getCurrentTarget() const;
    bool hasActiveJob() const;

    // Sincronización eficiente con los hilos
    bool isWorkQueueEmpty();
    std::mutex& getMutex();
    std::condition_variable& getConditionVariable();
    
    void shutdown();

private:
    JobManager();
    ~JobManager();

    // --- Métodos Internos ---
    void fetchIANoncesBackground();
    void processValidationResults(const std::vector<uint64_t>& nonces, const std::vector<bool>& results);
    std::vector<AnnotatedNonce> generateCpuNonces(size_t count);

    // Miembros
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_shutdown{false};

    MiningJob m_currentJob;
    std::atomic<bool> m_job_available{false};
    
    std::deque<AnnotatedNonce> m_cpuQueue;
    std::deque<AnnotatedNonce> m_iaQueue;

    std::atomic<float> m_aiContribution{0.5f};
    
    // Contadores y métricas
    std::atomic<size_t> m_validNonces{0};
    std::atomic<size_t> m_processedCount{0};

    // Hilo para la comunicación con la IA
    std::thread m_iaFetchThread;
    static constexpr auto IA_FETCH_INTERVAL = std::chrono::seconds(2);
};