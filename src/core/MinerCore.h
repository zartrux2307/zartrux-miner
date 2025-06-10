#pragma once



#include "crypto/randomx/randomx.h
#include "core/threads/WorkerThread.h"
#include "core/JobManager.h"
#include "core/NonceValidator.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <optional>
#include <string>
#include <chrono>


struct CheckpointState {
    uint64_t lastBlockHeight = 0;
    uint64_t totalHashes = 0;
    long acceptedShares = 0;
    std::string lastSeedHash;
    std::chrono::steady_clock::time_point miningStart;
};

class MinerCore {
public:
    struct WorkerStats {
        uint64_t totalHashes;
        uint64_t acceptedHashes;
        uint64_t iaNoncesUsed;
        double hashRate;
    };

    struct MiningConfig {
        std::optional<std::string> seed;
        unsigned threadCount = std::thread::hardware_concurrency();
        std::string mode;
        size_t noncePosition = 39;
        size_t nonceSize = 4;
        NonceValidator::Endianness nonceEndianness = NonceValidator::Endianness::LITTLE;
    };

    MinerCore(std::shared_ptr<JobManager> jobManager, unsigned threadCount = 0);
    ~MinerCore();

    bool initialize(const MiningConfig& config);
    void startMining();
    void stopMining();

    void setNumThreads(unsigned count);
    unsigned getNumThreads() const { return m_numThreads.load(); }
    
    bool isMining() const { return m_mining.load(); }
    long getMiningTime() const;
    int getActiveThreads() const;
    int getAcceptedShares() const;
    float getCurrentDifficulty() const;
    std::string getCurrentBlock() const;
    std::string getBlockStatus() const;
    float getTemperature() const;
    std::string getTempStatus() const;
    std::string getCurrentMode() const { return m_config.mode; }

    std::vector<WorkerStats> getWorkerStats() const;
    void updateMetrics();
    void saveCheckpoint() const;
    bool loadCheckpoint();

    // Nuevos: integración hooks y consola web/backend
    void broadcastEvent(const std::string& eventType, const std::string& payload) const;

private:
    void cleanupWorkers();
    void cleanupRandomX();
    void restartWorker(unsigned id);

    void setAffinity(unsigned threadId);

    MiningConfig m_config;
    std::shared_ptr<JobManager> m_jobManager;
    std::atomic<unsigned> m_numThreads;
    std::atomic<bool> m_mining;
    std::atomic<std::chrono::steady_clock::time_point> m_miningStartTime;
    std::atomic<long> m_acceptedShares;

    randomx_cache* m_rxCache = nullptr;
    std::vector<randomx_vm*> m_workerVMs;
    std::vector<std::unique_ptr<WorkerThread>> m_workers;
    mutable std::mutex m_workerMutex;

    // Checkpoint/estado persistente
    CheckpointState m_checkpoint;
};
