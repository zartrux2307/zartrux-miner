#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H



#include "SmartCache.h"
#include "NonceValidator.h"
#include "ia/IAReceiver.h"
#include "runtime/Profiler.h"
#include "utils/StatusExporter.h"
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <thread>
#include <fstream>




// Tipo para nonces con metadata
struct AnnotatedNonce {
    uint64_t value;
    float confidence = 1.0f;  // Confianza IA (1.0 = generado por CPU)
    uint64_t timestamp;
};

class JobManager {
public:
    static JobManager& getInstance();

    // Gestión de contribución IA
    void setAIContribution(float ratio);
    float getAIContribution() const;

    // Gestión de endpoints IA
    void setIAEndpoint(const std::string& endpoint);
    std::string getIAEndpoint() const;

    // Obtención de trabajo
    std::vector<AnnotatedNonce> getWorkBatch(size_t workerId, size_t maxNonces);

    // Inyección de nonces IA
    void injectIANonces(const std::vector<uint64_t>& nonces);

    // Validación y reporte
    void processNonces(const std::vector<uint64_t>& nonces, const std::vector<bool>& results);
    void submitValidNonce(uint64_t nonce, const std::string& hash);

    // Checkpoints y estado
    void saveCheckpoint();
    void loadCheckpoint();
    void shutdown();

private:
    JobManager();
    ~JobManager();

    // --- Métodos internos ---

    // ---- Gestión de Colas ----
    void fetchIANoncesBackground();
    void balanceQueues(size_t num_threads);
    std::vector<AnnotatedNonce> generateCpuNonces(size_t count);

    // ---- Lógica de Distribución ----
    void distributeToBatch(size_t workerId,
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
    static constexpr size_t LOG_ROTATE_EVERY = 100;      // nonces
    const std::chrono::milliseconds IA_FETCH_INTERVAL{2000};
};

#endif // JOB_MANAGER_H