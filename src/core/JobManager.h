#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <thread>
#include <fstream>
#include "SmartCache.h"
#include "NonceValidator.h"
#include "ia/IAReceiver.h"
#include "runtime/Profiler.h"

// ---- Para Prometheus/Backend hooks ----
#include "metrics/PrometheusExporter.h"
#include "utils/StatusExporter.h"

// Tipo para nonces con metadata
struct AnnotatedNonce {
    uint64_t value;
    float confidence = 1.0f;  // Confianza IA (1.0 = generado por CPU)
    uint64_t timestamp;
};

struct MiningJob {
    std::vector<uint8_t> blob;
    NonceValidator::hash_t target{};
    bool valid{false};
};

class JobManager {
public:
    static JobManager& getInstance();
    static JobManager& instance() { return getInstance(); }

    // Gestión de contribución IA
    void setAIContribution(float ratio);
    float getAIContribution() const;

    // Gestión de endpoints IA
    void setIAEndpoint(const std::string& endpoint);
    std::string getIAEndpoint() const;

      // Obtención de trabajo
    std::vector<AnnotatedNonce> getWorkBatch(size_t workerId, size_t maxNonces);
    MiningJob getCurrentJob() const;
    float getCurrentDifficulty() const;
    uint64_t getCurrentBlockHeight() const;
    bool isBlockValidating() const;

    // Inyección de nonces IA
    void injectIANonces(std::vector<AnnotatedNonce>&& nonces);

    // Reporte de resultados
    void reportProcessedNonces(const std::vector<std::pair<uint64_t, bool>>& results);

    // Registro de nonces válidos
    void submitValidNonce(uint64_t nonce, const std::string& hash);

    // Obtención de nonces desde IA (nueva implementación)
    std::vector<AnnotatedNonce> fetchNoncesFromIA();

    // Métricas y colas
    size_t getQueueSize() const;
    size_t getProcessedCount() const;

    // ---- Checkpoint & Recuperación ----
    void saveCheckpoint() const;
    void loadCheckpoint();

    // ---- Shutdown seguro ----
    void shutdown();

    // ---- Afinidad de hilos ----
    void setWorkerAffinity(size_t workerId, int cpuCore);

private:
    JobManager();
    ~JobManager();

    // Generación de nonces
    void generateNonces(size_t count);
    void fetchIANoncesBackground();

    // Distribución inteligente
    void distributeBatch(size_t workerId,
                         std::vector<AnnotatedNonce>& batch,
                         size_t maxNonces);

    // ---- Flood & Saturación ----
    bool floodControlActive(size_t queueCpu, size_t queueIa) const;

    // Miembros
    std::atomic<float> m_aiContribution{0.5f};
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    // Colas separadas para CPU e IA (deque: lock-free pop/push frontal/trasero)
    std::deque<AnnotatedNonce> m_cpuQueue;
    std::deque<AnnotatedNonce> m_iaQueue;

    // Trabajo actual
    MiningJob m_currentJob;
    std::atomic<float> m_currentDifficulty{0.0f};
    std::atomic<uint64_t> m_currentBlockHeight{0};
    std::atomic<bool> m_blockValidating{false};
    // Contadores
    std::atomic<size_t> m_validNonces{0};
    std::atomic<size_t> m_iaContributed{0};
    std::atomic<size_t> m_processedCount{0};

    // Rotación de logs
    std::atomic<size_t> m_validNoncesSinceLog{0};

    // Gestión IA
    std::string m_iaEndpoint;
    std::thread m_iaFetchThread;
    std::atomic<bool> m_shutdown{false};

    // Checkpoint
    std::string m_checkpointFile = "logs/jobmanager_checkpoint.dat";

    // Constantes
    static constexpr size_t MAX_QUEUE_SIZE = 250000;    // alta producción
    static constexpr size_t MAX_IA_QUEUE = 100000;
    static constexpr int MAX_RETRIES = 3;
    static constexpr std::chrono::milliseconds IA_FETCH_INTERVAL{1000};
    static constexpr size_t LOG_ROTATE_EVERY = 10000;

    // ---- Control flooding ----
    static constexpr size_t FLOOD_CPU_THRESHOLD = 240000;
    static constexpr size_t FLOOD_IA_THRESHOLD = 95000;
};

#endif // JOB_MANAGER_H
