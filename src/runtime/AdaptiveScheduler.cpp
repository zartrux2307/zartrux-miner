#include "AdaptiveScheduler.h"
#include "utils/Logger.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <stdexcept>

using namespace zartrux::runtime;

AdaptiveScheduler::AdaptiveScheduler(JobManager& jobManager, const WorkerThread::Config& workerConfig, size_t initialThreads)
    : jobManager_(jobManager), workerConfig_(workerConfig), targetHashRate_(0.0), powerLimit_(0.0)
{
    if (initialThreads == 0) {
        targetThreadCount_ = std::thread::hardware_concurrency();
        if (targetThreadCount_ == 0) targetThreadCount_ = 1;
    } else {
        targetThreadCount_ = initialThreads;
    }
}

AdaptiveScheduler::~AdaptiveScheduler() {
    stop();
}

void AdaptiveScheduler::start() {
    bool expected = false;
    if (!running_.load() && running_.compare_exchange_strong(expected, true)) {
        size_t threadsToLaunch = targetThreadCount_;
        for (size_t i = 0; i < threadsToLaunch; ++i) {
            unsigned int newId = (workers_.empty() ? 0 : workers_.back()->getId() + 1);
            workers_.emplace_back(std::make_unique<WorkerThread>(newId, jobManager_, workerConfig_));
            if (!affinity_.empty() && i < affinity_.size())
                workers_.back()->setAffinity(affinity_[i]); // soporte afinidad si WorkerThread lo implementa
            workers_.back()->start();
        }
        controlThread_ = std::make_unique<std::thread>(&AdaptiveScheduler::controlLoop, this);
        Logger::info("AdaptiveScheduler", "Lanzados " + std::to_string(workers_.size()) + " hilos de minado.");
    }
}

void AdaptiveScheduler::stop() {
    if (!running_.load()) return;
    running_.store(false);
    for (auto& worker : workers_) {
        worker->stop();
    }
    workers_.clear();
    if (controlThread_ && controlThread_->joinable()) {
        controlThread_->join();
    }
    Logger::info("AdaptiveScheduler", "Scheduler detenido.");
}

bool AdaptiveScheduler::isRunning() const {
    return running_.load();
}

void AdaptiveScheduler::setPerformanceTarget(double targetHashRate) {
    targetHashRate_ = targetHashRate;
}

void AdaptiveScheduler::setPowerLimit(double watts) {
    powerLimit_ = watts;
}

std::vector<AdaptiveScheduler::ThreadStats> AdaptiveScheduler::getThreadStats() const {
    std::vector<ThreadStats> stats;
    for (const auto& worker : workers_) {
        WorkerThread::Metrics m = worker->getMetrics();
        stats.push_back({ worker->getId(), m.hashRate, m.cpuUsage });
    }
    return stats;
}

void AdaptiveScheduler::setThreadAffinity(const std::vector<int>& cpuCores) {
    affinity_ = cpuCores;
    // Si ya hay hilos activos, actualiza la afinidad.
    for (size_t i = 0; i < workers_.size() && i < affinity_.size(); ++i) {
        workers_[i]->setAffinity(affinity_[i]);
    }
}

void AdaptiveScheduler::monitorSystem() {
    // [HOOK] Aquí puedes exportar métricas Prometheus/websocket para el backend.
    // Ejemplo: Logger::info("AdaptiveScheduler", "Monitor hook: export metrics ...");
}

void AdaptiveScheduler::restartWorker(size_t idx) {
    if (idx >= workers_.size()) return;
    try {
        unsigned int workerId = workers_[idx]->getId();
        workers_[idx]->stop();
        workers_[idx] = std::make_unique<WorkerThread>(workerId, jobManager_, workerConfig_);
        if (!affinity_.empty() && idx < affinity_.size())
            workers_[idx]->setAffinity(affinity_[idx]);
        workers_[idx]->start();
        Logger::warn("AdaptiveScheduler", "Reiniciado hilo de minería #" + std::to_string(workerId));
    } catch (const std::exception& ex) {
        Logger::error("AdaptiveScheduler", std::string("Error al reiniciar worker: ") + ex.what());
    }
}

void AdaptiveScheduler::adjustWorkers() {
    double totalHashRate = 0.0;
    double totalCpuUsage = 0.0;
    for (size_t i = 0; i < workers_.size(); ++i) {
        try {
            WorkerThread::Metrics m = workers_[i]->getMetrics();
            totalHashRate += m.hashRate;
            totalCpuUsage += m.cpuUsage;
            // Si el hilo reporta fallo crítico, lo reiniciamos (matrícula de honor).
            if (m.hasCriticalError) {
                restartWorker(i);
            }
        } catch (const std::exception& ex) {
            Logger::error("AdaptiveScheduler", std::string("Error en metrics/restart: ") + ex.what());
            restartWorker(i);
        }
    }
    size_t currentThreads = workers_.size();
    size_t maxThreads = targetThreadCount_;

    // [Throttling] Si el target cambia, ajusta rápidamente (matrícula de honor).
    while (currentThreads < maxThreads) {
        unsigned int newId = (currentThreads > 0 ? workers_.back()->getId() + 1 : 0);
        workers_.emplace_back(std::make_unique<WorkerThread>(newId, jobManager_, workerConfig_));
        if (!affinity_.empty() && currentThreads < affinity_.size())
            workers_.back()->setAffinity(affinity_[currentThreads]);
        workers_.back()->start();
        ++currentThreads;
        Logger::info("AdaptiveScheduler", "Aumentando hilos de minería a " + std::to_string(workers_.size()));
    }
    while (currentThreads > maxThreads && currentThreads > 1) {
        workers_.back()->stop();
        workers_.pop_back();
        --currentThreads;
        Logger::info("AdaptiveScheduler", "Reduciendo hilos de minería a " + std::to_string(workers_.size()));
    }

    // Ajuste dinámico basado en el hash rate.
    if (targetHashRate_ > 0.0) {
        if (totalHashRate < targetHashRate_ * 0.9 && currentThreads < maxThreads) {
            // Agregar un hilo si el hash rate es bajo.
            unsigned int newId = (currentThreads > 0 ? workers_.back()->getId() + 1 : 0);
            workers_.emplace_back(std::make_unique<WorkerThread>(newId, jobManager_, workerConfig_));
            if (!affinity_.empty() && currentThreads < affinity_.size())
                workers_.back()->setAffinity(affinity_[currentThreads]);
            workers_.back()->start();
            Logger::info("AdaptiveScheduler", "Aumentando hilos de minería a " + std::to_string(workers_.size()));
        } else if (totalHashRate > targetHashRate_ * 1.1 && currentThreads > 1) {
            // Remover el último hilo si hay exceso.
            workers_.back()->stop();
            workers_.pop_back();
            Logger::info("AdaptiveScheduler", "Reduciendo hilos de minería a " + std::to_string(workers_.size()));
        }
    }

    // Límite de potencia basado en uso de CPU.
    if (powerLimit_ > 0.0) {
        double allowedCpu = std::min(powerLimit_, 100.0);
        if (totalCpuUsage > allowedCpu * 1.1 && currentThreads > 1) {
            workers_.back()->stop();
            workers_.pop_back();
            Logger::warn("AdaptiveScheduler", "Límite de potencia: hilos disminuidos a " + std::to_string(workers_.size()));
        } else if (totalCpuUsage < allowedCpu * 0.5 && currentThreads < maxThreads) {
            unsigned int newId = (currentThreads > 0 ? workers_.back()->getId() + 1 : 0);
            workers_.emplace_back(std::make_unique<WorkerThread>(newId, jobManager_, workerConfig_));
            if (!affinity_.empty() && currentThreads < affinity_.size())
                workers_.back()->setAffinity(affinity_[currentThreads]);
            workers_.back()->start();
            Logger::info("AdaptiveScheduler", "Límite de potencia permite aumentar hilos a " + std::to_string(workers_.size()));
        }
    }
}

void AdaptiveScheduler::controlLoop() {
    const auto interval = std::chrono::seconds(2);
    while (running_.load()) {
        std::this_thread::sleep_for(interval);
        monitorSystem();
        adjustWorkers();
    }
}
