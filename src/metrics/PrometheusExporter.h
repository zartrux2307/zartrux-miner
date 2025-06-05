// src/metrics/PrometheusExporter.h

#pragma once
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/text_serializer.h>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <map>
#include <yaml-cpp/yaml.h>

namespace zartrux {

class PrometheusExporter {
public:
    enum class HashEventType { NewShare, HashRateUpdate, NonceProcessingTime };
    enum class NetworkEventType { NonceReceived, NonceValid, NonceInvalid };

    static PrometheusExporter& getInstance();
    
    void initialize(const std::string& listen_address = "0.0.0.0:9100",
                    const std::string& labels_yaml = "metrics/prometheus_labels.yaml");
    void shutdown() noexcept;
    
    void recordHashEvent(HashEventType type, double value = 0.0) noexcept;
    void recordNetworkEvent(NetworkEventType type, const std::string& source = "") noexcept;
    
    std::string exportMetrics() const;

    // Eliminar copias
    PrometheusExporter(const PrometheusExporter&) = delete;
    PrometheusExporter& operator=(const PrometheusExporter&) = delete;

    // Etiquetas parseadas del YAML
    std::map<std::string, std::string> getLabels() const { return labels_; }

private:
    PrometheusExporter() = default;
    ~PrometheusExporter() { shutdown(); }

    void registerHardwareMetrics();
    void registerCoreMetrics();
    void registerNetworkMetrics();
    void registerCustomMetrics();
    void updateSystemMetrics();
    void parseLabelsYAML(const std::string& yaml_path);

    // Métricas
    std::unique_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
    
    prometheus::Gauge* cpu_usage_ = nullptr;
    prometheus::Gauge* mem_usage_ = nullptr;
    prometheus::Gauge* gpu_usage_ = nullptr;
    prometheus::Gauge* temperature_ = nullptr;
    prometheus::Gauge* hashrate_ = nullptr;
    prometheus::Gauge* shares_ = nullptr;
    prometheus::Gauge* efficiency_ = nullptr;
    prometheus::Histogram* processing_time_ = nullptr;
    
    prometheus::Counter* received_nonces_ = nullptr;
    prometheus::Counter* valid_nonces_ = nullptr;
    prometheus::Counter* invalid_nonces_ = nullptr;

    // Estado
    std::string node_id_;
    bool initialized_{false};
    std::thread system_monitor_thread_;
    
    // Sincronización
    mutable std::mutex metrics_mutex_;
    std::mutex init_mutex_;
    static std::atomic<PrometheusExporter*> instance_;
    static std::mutex mutex_;

    std::map<std::string, std::string> labels_; // ← Etiquetas YAML
};

} // namespace zartrux
