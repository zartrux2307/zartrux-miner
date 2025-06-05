#pragma once

#include <cstdint>
#include <vector>
#include <chrono>
#include <string>
#include <algorithm>

namespace zartrux::runtime {

/**
 * @brief Enumera las características avanzadas del CPU relevantes para minería Monero/RandomX.
 */
enum class CPUFeature {
    SSE2,      ///< Requerido para RandomX eficiente.
    AVX,       ///< Mejora el rendimiento en CPUs modernos.
    AVX2,      ///< Alta eficiencia en minería RandomX.
    AVX512,    ///< No soportado por todos los CPUs, pero da máximo rendimiento.
    AES_NI,    ///< Acelera funciones internas de RandomX.
    BMI2       ///< Mejora ciertas operaciones en RandomX.
};

/**
 * @brief Estructura que describe el sistema en detalle.
 */
struct SystemInfo {
    std::string cpuName;              ///< Nombre completo del CPU.
    int physicalCores = 0;            ///< Núcleos físicos.
    int logicalCores = 0;             ///< Núcleos lógicos (con hyperthreading).
    double l3CacheSizeMB = 0.0;       ///< Tamaño caché L3 en MB (crítico para RandomX).
    double totalRamMB = 0.0;          ///< RAM física total en MB.
    std::vector<CPUFeature> features; ///< Características de CPU detectadas.
};

/**
 * @brief Herramientas de análisis de hardware y benchmarking para minería Monero (XMR).
 */
class Profiler {
public:
    /// Obtiene toda la info relevante del sistema para minería.
    [[nodiscard]] static SystemInfo getSystemInfo();

    /// ¿Tiene el CPU la feature relevante?
    [[nodiscard]] static bool hasFeature(CPUFeature feature);

    /// Lista todas las features presentes.
    [[nodiscard]] static std::vector<CPUFeature> detectSupportedFeatures();

    /// Convierte feature a string legible.
    [[nodiscard]] static std::string featureToString(CPUFeature feature);

    /**
     * @brief Resultado de un microbenchmark hash (usado solo para tuning y tests, nunca producción).
     */
    struct HashBenchmarkResult {
        double hashesPerSec = 0.0;
        double energyEfficiency = 0.0;
        std::chrono::nanoseconds avgLatency{};
        std::chrono::nanoseconds minLatency{};
        std::chrono::nanoseconds maxLatency{};
    };

    /**
     * @brief Ejecuta benchmark hash para autoajuste o diagnosis.
     * @warning *NO usar para comparar CPUs de diferentes arquitecturas, sólo tuning local.*
     */
    [[nodiscard]] static HashBenchmarkResult benchmarkHashPerf(
        const uint8_t* input,
        size_t len,
        size_t iterations,
        bool warmup = true);

    /**
     * @brief Monitoriza el rendimiento real del minero (ventana deslizante).
     */
    class PerformanceMonitor {
    public:
        explicit PerformanceMonitor(size_t windowSize = 60);

        /// Registrar nueva muestra de hashes/s.
        void recordSample(double hashesPerSec);

        /// Hashrate medio (últimos N segundos).
        [[nodiscard]] double getAverageHashRate() const;

        /// Factor de estabilidad [0,1] para autoajuste (1=estable).
        [[nodiscard]] double getStabilityFactor() const;

    private:
        std::vector<double> samples_; ///< Ventana circular.
        size_t currentIndex_ = 0;
        size_t samplesRecorded_ = 0;
    };
};

} // namespace zartrux::runtime
