#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include "utils/StatusExporter.h"

class JobManager {
public:
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    static constexpr size_t LOG_ROTATE_EVERY = 100;

    struct Job {
        std::vector<uint8_t> blob;
        std::string jobId;
        uint64_t target;
        uint32_t height;
    };

    struct Nonce {
        uint32_t value;
        std::string jobId;
    };

    JobManager();
    ~JobManager();

    void start();
    void stop();

    // Getters
    std::vector<uint8_t> getCurrentBlob() const;
    uint64_t getCurrentTarget() const;
    std::string getCurrentJobId() const;
    uint32_t getCurrentHeight() const;

    // Job management
    void setJob(const std::vector<uint8_t>& blob, const std::string& jobId, uint64_t target, uint32_t height);
    void submitNonce(uint32_t nonce);

    // AI/IA related functions
    void setAIContribution(float contribution);
    float getAIContribution() const;
    void setIAEndpoint(const std::string& endpoint);
    std::string getIAEndpoint() const;
    void injectIANonces(const std::vector<uint32_t>& nonces);
    void processNonces();

private:
    void loadCheckpoint();
    void saveCheckpoint();
    void submitValidNonce(uint32_t nonce, const std::string& jobId);
    void fetchIANoncesBackground();

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    // Current job data
    Job m_currentJob;
    std::atomic<bool> m_running{false};

    // Queues and counters
    std::queue<Nonce> m_cpuQueue;
    std::queue<Nonce> m_iaQueue;
    std::atomic<uint64_t> m_processedCount{0};
    std::atomic<uint64_t> m_validNonces{0};
    std::atomic<uint64_t> m_validNoncesSinceLog{0};
    std::atomic<uint64_t> m_iaContributed{0};

    // IA/AI configuration
    std::string m_iaEndpoint;
    float m_aiContribution{0.0f};

    // Status exporter
    StatusExporter m_statusExporter;
};