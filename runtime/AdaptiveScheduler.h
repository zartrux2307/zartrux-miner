#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "Profiler.h"
#include "core/threads/WorkerThread.h"

class JobManager;

namespace zartrux::runtime {

/**
 * @brief AdaptiveScheduler para minería real de Monero (XMR) usando RandomX.
 *        Ajusta dinámicamente el número de hilos de minado según el rendimiento
 *        y condiciones del sistema. 100% exclusivo para XMR.
 */
class AdaptiveScheduler {
public:
    struct ThreadStats {
        size_t threadId;
        double hashRate;
        double cpuUsage;
    };

    /**
     * @param jobManager Referencia al gestor de trabajos de minería.
     * @param workerConfig Configuración por hilo.
     * @param initialThreads Número inicial de hilos (por defecto: detecta hardware).
     */
    AdaptiveScheduler(JobManager& jobManager,
                      const WorkerThread::Config& workerConfig,
                      size_t initialThreads = 0);

    ~AdaptiveScheduler();

    /** Inicia el scheduler y los hilos de minado. */
    void start();

    /** Detiene todos los hilos y limpia el scheduler. */
    void stop();

    /** ¿Está en ejecución? */
    bool isRunning() const;

    /** Meta de rendimiento en H/s (hashes por segundo). */
    void setPerformanceTarget(double targetHashRate);

    /** Límite de consumo energético (watts). */
    void setPowerLimit(double watts);

    /** Métricas en tiempo real de los hilos de minado. */
    std::vector<ThreadStats> getThreadStats() const;

    /** Número actual de hilos activos. */
    size_t getMaxThreads() const;

    /** Actualiza el número objetivo de hilos (para throttling). */
    void setTargetThreadCount(size_t count);

    /** [MATRÍCULA] Hook: Fija la afinidad de cada hilo a un core específico. */
    void setThreadAffinity(const std::vector<int>& cpuCores);

    AdaptiveScheduler(const AdaptiveScheduler&) = delete;
    AdaptiveScheduler& operator=(const AdaptiveScheduler&) = delete;

private:
    void controlLoop();
    void adjustWorkers();
    void monitorSystem();
    void restartWorker(size_t idx);

    JobManager& jobManager_;
    WorkerThread::Config workerConfig_;
    std::vector<std::unique_ptr<WorkerThread>> workers_;
    std::unique_ptr<std::thread> controlThread_;
    std::atomic<bool> running_{false};
    size_t targetThreadCount_{0};
    double targetHashRate_{0};
    double powerLimit_{0};
    Profiler::PerformanceMonitor perfMonitor_{32};
    std::vector<int> affinity_; // Afinidad opcional de hilos (CPU ids)
};

inline size_t AdaptiveScheduler::getMaxThreads() const {
    return workers_.size();
}

inline void AdaptiveScheduler::setTargetThreadCount(size_t count) {
    targetThreadCount_ = count;
}

} // namespace zartrux::runtime
