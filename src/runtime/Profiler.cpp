#include "Profiler.h"
#include <numeric>
#include <cmath>
#include <immintrin.h>
#include <fstream>
#include <thread>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstring>

#ifdef _WIN32
    #include <intrin.h>
    #include <windows.h>
    #undef ERROR
    #undef INFO
#else
    #include <cpuid.h>
    #include <unistd.h>
#endif

namespace zartrux::runtime {

SystemInfo Profiler::getSystemInfo() {
    SystemInfo info;
    char cpuName[0x40] = {0};
#if defined(__i386__) || defined(__x86_64__)
    // Extrae nombre CPU
    for (int i = 0; i < 3; i++) {
        unsigned int regs[4] = {0};
#ifdef _WIN32
        __cpuid(reinterpret_cast<int*>(regs), 0x80000002 + i);
#else
        __get_cpuid(0x80000002 + i, &regs[0], &regs[1], &regs[2], &regs[3]);
#endif
        std::memcpy(cpuName + i * 16, regs, 16);
    }
    info.cpuName = cpuName;

    unsigned int eax, ebx, ecx, edx;

#ifdef _WIN32
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0x80000008);
    ecx = cpuInfo[2];
    info.physicalCores = ((ecx >> 12) & 0xF) + 1;

    __cpuid(cpuInfo, 0x80000004);
    eax = cpuInfo[0];
    info.logicalCores = (eax & 0xFF) + 1;

    __cpuid(cpuInfo, 0x80000006);
    ecx = cpuInfo[2];
    info.l3CacheSizeMB = ((ecx >> 16) & 0xFFFF) / 1024.0;
#else
    __get_cpuid(0x80000008, &eax, &ebx, &ecx, &edx);
    info.physicalCores = ((ecx >> 12) & 0xF) + 1;
    info.logicalCores = (eax & 0xFF) + 1;

    __get_cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    info.l3CacheSizeMB = ((ecx >> 16) & 0xFFFF) / 1024.0;
#endif

#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);
    info.totalRamMB = memInfo.ullTotalPhys / (1024.0 * 1024);
#elif defined(__linux__)
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    info.totalRamMB = (pages * page_size) / (1024 * 1024);
#else
    info.totalRamMB = 0;
#endif

    info.features = detectSupportedFeatures();
#endif
    return info;
}

bool Profiler::hasFeature(CPUFeature feature) {
    static const auto features = detectSupportedFeatures();
    return std::find(features.begin(), features.end(), feature) != features.end();
}

std::vector<CPUFeature> Profiler::detectSupportedFeatures() {
    std::vector<CPUFeature> features;
#if defined(__i386__) || defined(__x86_64__)
    unsigned int eax, ebx, ecx, edx;

#ifdef _WIN32
    int regs[4] = {0};
    __cpuid(regs, 1);
    edx = regs[3];
    ecx = regs[2];
#else
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
#endif
    if (edx & (1 << 26)) features.push_back(CPUFeature::SSE2);
    if (ecx & (1 << 25)) features.push_back(CPUFeature::AES_NI);
    if (ecx & (1 << 28)) features.push_back(CPUFeature::AVX);

#ifdef _WIN32
    __cpuidex(regs, 7, 0);
    ebx = regs[1];
#else
    __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
#endif
    if (ebx & (1 << 5))  features.push_back(CPUFeature::AVX2);
    if (ebx & (1 << 16)) features.push_back(CPUFeature::AVX512);
    if (ebx & (1 << 8))  features.push_back(CPUFeature::BMI2);
#endif
    return features;
}

std::string Profiler::featureToString(CPUFeature feature) {
    switch (feature) {
        case CPUFeature::SSE2:   return "SSE2";
        case CPUFeature::AVX:    return "AVX";
        case CPUFeature::AVX2:   return "AVX2";
        case CPUFeature::AVX512: return "AVX512";
        case CPUFeature::AES_NI: return "AES-NI";
        case CPUFeature::BMI2:   return "BMI2";
        default:                 return "Unknown";
    }
}

Profiler::HashBenchmarkResult Profiler::benchmarkHashPerf(
    const uint8_t* input,
    size_t len,
    size_t iterations,
    bool warmup) {

    HashBenchmarkResult result;
    std::vector<std::chrono::nanoseconds> timings;
    timings.reserve(iterations);

    // *** Usar solo para tuning/debug ***
    if (warmup) {
        std::vector<uint8_t> dummy(32);
        for (int i = 0; i < 100; i++) {
            // Usar SHA256 como referencia, nunca RandomX aquí
            std::fill(dummy.begin(), dummy.end(), 0);
        }
    }

    auto totalStart = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++) {
        std::vector<uint8_t> output(32);
        auto start = std::chrono::high_resolution_clock::now();
        // Hash genérico sólo para control, nunca para la minería real
        std::fill(output.begin(), output.end(), 0);
        auto end = std::chrono::high_resolution_clock::now();
        timings.emplace_back(end - start);
    }
    auto totalEnd = std::chrono::high_resolution_clock::now();

    auto totalTime = totalEnd - totalStart;
    result.hashesPerSec = (iterations * 1e9) / totalTime.count();
    result.avgLatency = std::chrono::nanoseconds(totalTime.count() / iterations);
    result.minLatency = *std::min_element(timings.begin(), timings.end());
    result.maxLatency = *std::max_element(timings.begin(), timings.end());
    result.energyEfficiency = 0.0;

    return result;
}

Profiler::PerformanceMonitor::PerformanceMonitor(size_t windowSize)
    : samples_(windowSize), currentIndex_(0), samplesRecorded_(0) {}

void Profiler::PerformanceMonitor::recordSample(double hashesPerSec) {
    samples_[currentIndex_] = hashesPerSec;
    currentIndex_ = (currentIndex_ + 1) % samples_.size();
    samplesRecorded_++;
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
    double variance = 0.0;
    for (size_t i = 0; i < count; i++) {
        variance += std::pow(samples_[i] - mean, 2);
    }
    double stddev = std::sqrt(variance / count);
    return 1.0 / (1.0 + stddev / mean);
}

} // namespace zartrux::runtime
