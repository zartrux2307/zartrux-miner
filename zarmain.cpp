#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <csignal>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

#include "utils/Logger.h"
#include "core/MinerCore.h"
#include "core/JobManager.h"
#include "core/NonceValidator.h"
#include "network/PoolDispatcher.h"
#include "metrics/PrometheusExporter.h"
#include "utils/ConfigManager.h"
#include "utils/Profiler.h"
#include "utils/StatusExporter.h"

using json = nlohmann::json;
using namespace std::chrono;
namespace fs = std::filesystem;

// Variables globales
std::atomic<bool> g_running = true;
std::unique_ptr<MinerCore> g_miner;
std::unique_ptr<JobManager> g_jobManager;
std::unique_ptr<PoolDispatcher> g_poolDispatcher;
std::shared_ptr<ConfigManager> g_config;

// Estructura para estado del minero
struct MinerStatus {
    double hashrate = 0.0;
    uint64_t shares = 0;
    float temperature = 0.0f;
    int64_t mining_seconds = 0;
    int active_threads = 0;
    int total_threads = 0;

    json toJson() const {
        return {
            {"hashrate", hashrate},
            {"shares", shares},
            {"temperature", temperature},
            {"mining_seconds", mining_seconds},
            {"active_threads", active_threads},
            {"total_threads", total_threads}
        };
    }

    static MinerStatus fromJson(const json& j) {
        MinerStatus status;
        status.hashrate = j.value("hashrate", 0.0);
        status.shares = j.value("shares", 0ULL);
        status.temperature = j.value("temperature", 0.0f);
        status.mining_seconds = j.value("mining_seconds", 0LL);
        status.active_threads = j.value("active_threads", 0);
        status.total_threads = j.value("total_threads", 0);
        return status;
    }
};

// Manejador de señales
void signalHandler(int signum) {
    Logger::info("Main", "Señal recibida: {}", signum);
    g_running = false;
}

// Inicialización
bool initialize() {
    try {
        // Configurar manejador de señales
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        // Cargar configuración
        g_config = std::make_shared<ConfigManager>("config.json");
        if (!g_config->load()) {
            Logger::error("Main", "Error al cargar configuración");
            return false;
        }

        // Inicializar componentes principales
        g_jobManager = std::make_unique<JobManager>();
        g_poolDispatcher = std::make_unique<PoolDispatcher>(*g_jobManager);

        // Configurar minero
        MinerCore::MiningConfig minerConfig;
        minerConfig.threadCount = g_config->get<unsigned>("threads", std::thread::hardware_concurrency());
        minerConfig.mode = g_config->get<std::string>("mining_mode", "normal");
        g_miner = std::make_unique<MinerCore>(g_jobManager, minerConfig.threadCount);

        if (!g_miner->initialize(minerConfig)) {
            Logger::error("Main", "Error al inicializar minero");
            return false;
        }

        // Configurar métricas
        PrometheusExporter::getInstance().initialize(
            g_config->get<uint16_t>("metrics_port", 9100));

        // Restaurar estado si existe
        if (fs::exists("miner_state.json")) {
            std::ifstream stateFile("miner_state.json");
            json stateJson;
            stateFile >> stateJson;
            auto status = MinerStatus::fromJson(stateJson);
            g_miner->restoreState(status);
        }

        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Main", "Error en inicialización: {}", e.what());
        return false;
    }
}

// Bucle principal
void mainLoop() {
    auto nextMetricsUpdate = steady_clock::now();
    auto nextPoolCheck = steady_clock::now();
    auto nextConfigCheck = steady_clock::now();
    auto nextProfilerUpdate = steady_clock::now();
    
    Profiler profiler;

    while (g_running) {
        try {
            auto now = steady_clock::now();

            // Actualizar configuración
            if (now >= nextConfigCheck) {
                if (g_config->checkForChanges()) {
                    std::string newMode = g_config->get<std::string>("mining_mode", "normal");
                    g_miner->setMiningMode(newMode);
                }
                nextConfigCheck = now + seconds(30);
            }

            // Perfilado de rendimiento
            if (now >= nextProfilerUpdate) {
                profiler.update();
                Logger::debug("Main", "CPU: {:.1f}%, Memoria: {:.1f}MB",
                            profiler.getCPUUsage(),
                            profiler.getMemoryUsageMB());
                nextProfilerUpdate = now + seconds(5);
            }

            // Actualizar métricas
            if (now >= nextMetricsUpdate) {
                updateMetrics();
                nextMetricsUpdate = now + milliseconds(1000);
                PrometheusExporter::getInstance().update();
            }

            // Verificar conexión a pool
            if (now >= nextPoolCheck) {
                g_poolDispatcher->checkConnection();
                nextPoolCheck = now + seconds(5);
            }

            // Dormir hasta próxima actualización
            auto nextUpdate = std::min({
                nextMetricsUpdate,
                nextPoolCheck,
                nextConfigCheck,
                nextProfilerUpdate
            });
            
            auto sleepTime = duration_cast<milliseconds>(nextUpdate - steady_clock::now());
            if (sleepTime.count() > 0) {
                std::this_thread::sleep_for(sleepTime);
            }
        }
        catch (const std::exception& e) {
            Logger::error("Main", "Error en bucle principal: {}", e.what());
            std::this_thread::sleep_for(seconds(1));
        }
    }
}

// Limpieza
void cleanup() {
    try {
        if (g_miner) {
            g_miner->stopMining();
            
            // Guardar estado
            std::ofstream stateFile("miner_state.json");
            MinerStatus status;
            status.hashrate = g_miner->getHashRate();
            status.shares = g_miner->getAcceptedShares();
            stateFile << status.toJson().dump(4);
        }

        PrometheusExporter::getInstance().shutdown();
        Logger::info("Main", "Limpieza completada");
    }
    catch (const std::exception& e) {
        Logger::error("Main", "Error en limpieza: {}", e.what());
    }
}

int main(int argc, char* argv[]) {
    try {
        // Inicializar logger
        Logger::init("zartrux-miner.log", Logger::Level::Debug);
        Logger::info("Main", "Iniciando zartrux-miner v1.0.0");

        // Inicializar componentes
        if (!initialize()) {
            Logger::error("Main", "Fallo en la inicialización");
            return 1;
        }

        // Iniciar minería
        g_miner->startMining();
        Logger::info("Main", "Minería iniciada");

        // Bucle principal
        mainLoop();

        // Limpieza
        cleanup();
        return 0;
    }
    catch (const std::exception& e) {
        Logger::error("Main", "Error fatal: {}", e.what());
        return 1;
    }
}