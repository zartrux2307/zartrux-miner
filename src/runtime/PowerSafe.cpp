#include "PowerSafe.h"
#include "AdaptiveScheduler.h"
#include "utils/Logger.h" // Ajusta la ruta si es necesario
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <functional>
#include <string>

namespace zartrux::runtime {

// Implementación concreta de la interfaz PowerSafe.
class PowerSafeDefault : public PowerSafe {
public:
    PowerSafeDefault()
        : temperatureLimit_(80.0),   // Límite por defecto en °C
          powerLimit_(100.0),        // Límite por defecto en vatios
          currentMode_(PowerMode::BALANCED),
          emergencyShutdown_(false),
          monitoringActive_(false)
    {}

    ~PowerSafeDefault() override {
        stopMonitoring();
    }

    void setTemperatureLimit(double celsius) override {
        temperatureLimit_ = celsius;
    }

    void setPowerMode(PowerMode mode) override {
        currentMode_ = mode;
    }

    void setPowerLimit(double watts) override {
        powerLimit_ = watts;
    }

    void adjustPowerState() override {
        double currentTemp = tempMonitor_ ? tempMonitor_() : 0.0;
        double currentPower = powerMonitor_ ? powerMonitor_() : 0.0;

      Logger::info("PowerSafeDefault", "Temp: {} °C, Power: {} W", currentTemp, currentPower);

        if (currentTemp > temperatureLimit_ + 10.0) {
            emergencyShutdown_ = true;
             Logger::error("PowerSafeDefault", "EMERGENCY SHUTDOWN! Temp over hard limit.");
        }

        if (scheduler_) {
            if (currentTemp > temperatureLimit_) {
                size_t newTarget = scheduler_->getMaxThreads() > 1 ? scheduler_->getMaxThreads() - 1 : 1;
                scheduler_->setTargetThreadCount(newTarget);
                 Logger::warn("PowerSafeDefault", "Reducing threads (temp high). New thread count: {}", newTarget);
            }
            else if (currentPower > powerLimit_) {
                size_t newTarget = scheduler_->getMaxThreads() > 1 ? scheduler_->getMaxThreads() - 1 : 1;
                scheduler_->setTargetThreadCount(newTarget);
                Logger::warn("PowerSafeDefault", "Reducing threads (power high). New thread count: {}", newTarget);
            }
            else if (currentTemp < temperatureLimit_ - 5.0 && currentPower < powerLimit_ * 0.8) {
                size_t newTarget = scheduler_->getMaxThreads() + 1;
                scheduler_->setTargetThreadCount(newTarget);
                 Logger::info("PowerSafeDefault", "Increasing threads (conditions optimal). New thread count: {}", newTarget);
            }
        }
    }

    void startMonitoring(std::function<double()> tempMonitor,
                         std::function<double()> powerMonitor,
                         std::shared_ptr<AdaptiveScheduler> scheduler) override {
        tempMonitor_ = tempMonitor;
        powerMonitor_ = powerMonitor;
        scheduler_ = scheduler;
        monitoringActive_ = true;
        monitoringThread_ = std::make_unique<std::thread>(&PowerSafeDefault::monitorLoop, this);
    }

    void stopMonitoring() override {
        monitoringActive_ = false;
        if (monitoringThread_ && monitoringThread_->joinable()) {
            monitoringThread_->join();
        }
    }

    bool isEmergencyShutdown() const override {
        return emergencyShutdown_;
    }

private:
    void monitorLoop() {
        while (monitoringActive_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            adjustPowerState();
        }
    }

    double temperatureLimit_;
    double powerLimit_;
    PowerMode currentMode_;

    std::function<double()> tempMonitor_;
    std::function<double()> powerMonitor_;
    std::shared_ptr<AdaptiveScheduler> scheduler_;

    std::unique_ptr<std::thread> monitoringThread_;

    std::atomic<bool> monitoringActive_;
    std::atomic<bool> emergencyShutdown_;
};

// Fábrica recomendada para integración modular
std::shared_ptr<PowerSafe> createPowerSafe() {
    return std::make_shared<PowerSafeDefault>();
}

} // namespace zartrux::runtime
