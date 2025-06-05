// src/metrics/PrometheusExporter.cpp

#include "PrometheusExporter.h"
#include <fstream>
#include <iostream>
#include <prometheus/text_serializer.h>
#include <yaml-cpp/yaml.h>
#include "runtime/SystemMonitor.h"

namespace zartrux {

std::atomic<PrometheusExporter*> PrometheusExporter::instance_{nullptr};
std::mutex PrometheusExporter::mutex_;

PrometheusExporter& PrometheusExporter::getInstance() {
    PrometheusExporter* tmp = instance_.load(std::memory_order_acquire);
    if (!tmp) {
        std::lock_guard<std::mutex> lock(mutex_);
        tmp = instance_.load(std::memory_order_relaxed);
        if (!tmp) {
            tmp = new PrometheusExporter();
            instance_.store(tmp, std::memory_order_release);
        }
    }
    return *tmp;
}

void PrometheusExporter::initialize(const std::string& listen_address, const std::string& labels_yaml) {
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (initialized_) return;
    exposer_ = std::make_unique<prometheus::Exposer>(listen_address);
    registry_ = std::make_shared<prometheus::Registry>();
    parseLabelsYAML(labels_yaml);

    registerHardwareMetrics();
    registerCoreMetrics();
    registerNetworkMetrics();
    exposer_->RegisterCollectable(registry_);
    initialized_ = true;

    // Lanzar hilo de actualización de sistema
    system_monitor_thread_ = std::thread([this] {
        while (initialized_) {
            updateSystemMetrics();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });
}

void PrometheusExporter::shutdown() noexcept {
    initialized_ = false;
    if (system_monitor_thread_.joinable())
        system_monitor_thread_.join();
    exposer_.reset();
    registry_.reset();
}

void PrometheusExporter::registerHardwareMetrics() {
    auto& family = prometheus::BuildGauge()
        .Name("zartrux_cpu_usage").Help("CPU Usage (%)")
        .Register(*registry_);
    cpu_usage_ = &family.Add(labels_);
    auto& family2 = prometheus::BuildGauge()
        .Name("zartrux_memory_usage").Help("RAM Usage (GB)")
        .Register(*registry_);
    mem_usage_ = &family2.Add(labels_);
    auto& family3 = prometheus::BuildGauge()
        .Name("zartrux_temperature").Help("CPU Temperature (°C)")
        .Register(*registry_);
    temperature_ = &family3.Add(labels_);
}

void PrometheusExporter::registerCoreMetrics() {
    auto& hrate = prometheus::BuildGauge()
        .Name("zartrux_hashrate").Help("Hashrate (H/s)")
        .Register(*registry_);
    hashrate_ = &hrate.Add(labels_);
    auto& shares = prometheus::BuildGauge()
        .Name("zartrux_shares").Help("Shares válidas")
        .Register(*registry_);
    shares_ = &shares.Add(labels_);
    auto& eff = prometheus::BuildGauge()
        .Name("zartrux_efficiency").Help("Eficiencia")
        .Register(*registry_);
    efficiency_ = &eff.Add(labels_);
    auto& proc_time = prometheus::BuildHistogram()
        .Name("zartrux_nonce_processing_time")
        .Help("Nonce processing time (ms)")
        .Register(*registry_);
    processing_time_ = &proc_time.Add(labels_, prometheus::Histogram::BucketBoundaries{1, 5, 10, 50, 100, 200, 1000});
}

void PrometheusExporter::registerNetworkMetrics() {
    auto& recv = prometheus::BuildCounter()
        .Name("zartrux_nonces_received")
        .Help("Nonces recibidos").Register(*registry_);
    received_nonces_ = &recv.Add(labels_);
    auto& valid = prometheus::BuildCounter()
        .Name("zartrux_nonces_valid")
        .Help("Nonces válidos").Register(*registry_);
    valid_nonces_ = &valid.Add(labels_);
    auto& invalid = prometheus::BuildCounter()
        .Name("zartrux_nonces_invalid")
        .Help("Nonces inválidos").Register(*registry_);
    invalid_nonces_ = &invalid.Add(labels_);
}

void PrometheusExporter::registerCustomMetrics() {
    // Aquí puedes añadir métricas extra
}

void PrometheusExporter::updateSystemMetrics() {
    auto data = SystemMonitor::getSystemData();
    if (cpu_usage_) cpu_usage_->Set(data.cpu_usage);
    if (mem_usage_) mem_usage_->Set(data.ram_usage);
    if (temperature_) temperature_->Set(data.cpu_temp);
}

void PrometheusExporter::recordHashEvent(HashEventType type, double value) noexcept {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    switch (type) {
        case HashEventType::NewShare: if (shares_) shares_->Increment(); break;
        case HashEventType::HashRateUpdate: if (hashrate_) hashrate_->Set(value); break;
        case HashEventType::NonceProcessingTime: if (processing_time_) processing_time_->Observe(value); break;
    }
}

void PrometheusExporter::recordNetworkEvent(NetworkEventType type, const std::string&) noexcept {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    switch (type) {
        case NetworkEventType::NonceReceived: if (received_nonces_) received_nonces_->Increment(); break;
        case NetworkEventType::NonceValid: if (valid_nonces_) valid_nonces_->Increment(); break;
        case NetworkEventType::NonceInvalid: if (invalid_nonces_) invalid_nonces_->Increment(); break;
    }
}

std::string PrometheusExporter::exportMetrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    prometheus::TextSerializer serializer;
    return serializer.Serialize(*registry_.get());
}

void PrometheusExporter::parseLabelsYAML(const std::string& yaml_path) {
    labels_.clear();
    try {
        YAML::Node config = YAML::LoadFile(yaml_path);
        for (auto it = config.begin(); it != config.end(); ++it) {
            labels_[it->first.as<std::string>()] = it->second.as<std::string>();
        }
    } catch (...) {
        // Default label
        labels_["zartrux_node"] = "unknown";
    }
}

} // namespace zartrux
