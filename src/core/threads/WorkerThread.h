#pragma once

#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstdint>
#include "core/JobManager.h"
#include "core/NonceValidator.h"
class WorkerThread {
public:
    struct Metrics {
        std::atomic<double> hashRate{0};
        std::atomic<double> cpuUsage{0};
        std::atomic<uint64_t> totalHashes{0};
        std::atomic<uint64_t> acceptedHashes{0};
        std::atomic<uint64_t> iaNoncesUsed{0};
        std::atomic<bool> hasCriticalError{false};
        Metrics() = default;
    };

    struct Config {
        void* vm;
        int cpuAffinity = -1;
        double throttle = 1.0;
        size_t noncePosition = 39;
        size_t nonceSize = 8;
        bool nonceEndianness = false;
    };

    /// Constructor/destructor
    WorkerThread(unsigned id, JobManager& jobManager, const Config& config);
    ~WorkerThread();

    /// Iniciar hilo de minería
    void start();
    /// Detener hilo de minería
    void stop();
    /// Reiniciar hilo (stop + start)
    void restart();
      /// Afinidad de CPU (bindear hilo a core)
    bool setCPUAffinity(int core);

    bool isRunning() const;
    Metrics getMetrics() const;
    unsigned getId() const { return m_id; }
    void setAffinity(int core) { m_config.cpuAffinity = core; }

private:
    void run();
    std::string toHexString(const NonceValidator::hash_t& hash) const;

    unsigned m_id;
    JobManager& m_jobManager;
    Config m_config;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    mutable Metrics m_metrics;
    std::atomic<bool> m_hybridToggle{false};
};
