#include "runtime/Profiler.h"
#include "utils/Logger.h"
#include <numeric>
#include <cmath>
#include <fstream>
#include <thread>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstring>
#include <limits>

#ifdef _WIN32
    #include <intrin.h>
    #include <windows.h>
#else
    #include <cpuid.h>
    #include <unistd.h>
#endif

namespace zartrux::runtime {

SystemInfo Profiler::getSystemInfo() {
    SystemInfo info;
    char cpuNameBuffer[0x40] = {0};

#if defined(__i386__) || defined(__x86_64__)
    for (int i = 0; i < 3; i++) {
        unsigned int regs[4] = {0};
#ifdef _WIN32
        __cpuid(reinterpret_cast<int*>(regs), (int)(0x80000002 + i));
#else
        __get_cpuid(0x80000002 + i, &regs[0], &regs[1], &regs[2], &regs[3]);
#endif
        std::memcpy(cpuNameBuffer + i * 16, regs, 16);
    }
    info.cpuName = cpuNameBuffer;
#endif
    
    // --- CORRECCIÓN (Advertencia C4189): Usamos la variable para que no esté sin referenciar ---
    Logger::info("Profiler", "CPU detectada: %s", info.cpuName.c_str());

    return info;
}

// Inicialización de miembros estáticos
std::mutex Profiler::m_mutex;
std::map<std::string, Profiler::ProfileData> Profiler::m_profiles;

void Profiler::start(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profiles[name].last_start_time = std::chrono::high_resolution_clock::now();
}

void Profiler::stop(const std::string& name) {
    auto end_time = std::chrono::high_resolution_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_profiles.find(name);
    if (it == m_profiles.end()) {
        return; 
    }

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - it->second.last_start_time).count();
    it->second.total_duration += duration;
    it->second.call_count++;

    if (it->second.durations.size() < 1000) { // Limitar el número de muestras
        it->second.durations.push_back(duration);
    }
}

void Profiler::printReport() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Logger::info("PROFILER REPORT", "======================================================================");
    Logger::info("PROFILER REPORT", "%-25s | %10s | %10s | %10s | %12s", "Function Name", "Avg (ms)", "Min (ms)", "Max (ms)", "Total Calls");
    Logger::info("PROFILER REPORT", "--------------------------|------------|------------|------------|---------------");

    for (const auto& pair : m_profiles) {
        const auto& name = pair.first;
        const auto& data = pair.second;

        if (data.call_count == 0) continue;

        double avg = (static_cast<double>(data.total_duration) / data.call_count) / 1e6;
        
        long long min_val = data.durations.empty() ? 0 : *std::min_element(data.durations.begin(), data.durations.end());
        long long max_val = data.durations.empty() ? 0 : *std::max_element(data.durations.begin(), data.durations.end());
        
        Logger::info("PROFILER REPORT", "%-25s | %10.4f | %10.4f | %10.4f | %12llu",
                     name.c_str(),
                     avg,
                     static_cast<double>(min_val) / 1e6,
                     static_cast<double>(max_val) / 1e6,
                     data.call_count);
    }
    Logger::info("PROFILER REPORT", "======================================================================");
}


// --- ScopeProfiler ---
ScopeProfiler::ScopeProfiler(const std::string& name) : m_name(name) {
    Profiler::start(m_name);
}

ScopeProfiler::~ScopeProfiler() {
    Profiler::stop(m_name);
}


// --- PerformanceResult ---
void Profiler::PerformanceResult::print() const {
    Logger::info("BenchmarkResult", "Iteraciones: %llu", iterations);
    Logger::info("BenchmarkResult", "Tiempo Total: %.2f ms", totalTime.count() / 1e6);
    Logger::info("BenchmarkResult", "Hashrate: %.2f H/s", hashesPerSec);
    Logger::info("BenchmarkResult", "Latencia Avg/Min/Max: %.2f / %.2f / %.2f us",
        avgLatency.count() / 1e3,
        minLatency.count() / 1e3,
        maxLatency.count() / 1e3
    );
}


// --- PerformanceMonitor ---

Profiler::PerformanceResult Profiler::PerformanceMonitor::run(std::function<void()> benchmarkable, uint64_t iterations) {
    if (iterations == 0) {
        iterations = (std::numeric_limits<uint64_t>::max)(); // <-- CORRECCIÓN: Paréntesis extra eliminados
    }

    std::vector<std::chrono::nanoseconds> timings;
    timings.reserve(iterations > 10000 ? 10000 : iterations); // Limitar la reserva

    auto totalStart = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < iterations; ++i) {
        auto iterStart = std::chrono::high_resolution_clock::now();
        benchmarkable();
        auto iterEnd = std::chrono::high_resolution_clock::now();
        timings.push_back(iterEnd - iterStart);
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();

    PerformanceResult result;
    result.iterations = iterations;
    result.totalTime = totalEnd - totalStart;
    result.hashesPerSec = (iterations * 1e9) / result.totalTime.count();
    result.avgLatency = std::chrono::nanoseconds(result.totalTime.count() / iterations);
    result.minLatency = *std::min_element(timings.begin(), timings.end());
    result.maxLatency = *std::max_element(timings.begin(), timings.end());
    result.energyEfficiency = 0.0; // Implementar si se dispone de monitor de energía

    return result;
}

Profiler::PerformanceMonitor::PerformanceMonitor(size_t windowSize)
    : samples_(windowSize), currentIndex_(0), samplesRecorded_(0) {}

void Profiler::PerformanceMonitor::recordSample(double hashesPerSec) {
    samples_[currentIndex_] = hashesPerSec;
    currentIndex_ = (currentIndex_ + 1) % samples_.size();
    if (samplesRecorded_ < samples_.size()) {
        samplesRecorded_++;
    }
}

double Profiler::PerformanceMonitor::getAverageHashRate() const {
    if (samplesRecorded_ == 0) return 0.0;
    size_t count = std::min(samplesRecorded_, samples_.size());
    return std::accumulate(samples_.begin(), samples_.begin() + count, 0.0) / count;
}

double Profiler::PerformanceMonitor::getStabilityFactor() const {
    if (samplesRecorded_ < 2) return 1.0;
    size_t count = std::min(samplesRecorded_, samples_.size());
    double mean = getAverageHashRate();
    double sq_sum = std::accumulate(samples_.begin(), samples_.begin() + count, 0.0, 
        [mean](double accumulator, double val) {
            return accumulator + (val - mean) * (val - mean);
        });
    double std_dev = std::sqrt(sq_sum / count);
    return (mean > 0) ? (1.0 - (std_dev / mean)) : 0.0;
}


} // namespace zartrux::runtime