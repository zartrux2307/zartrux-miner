#include "core/MinerCore.h"
#include "core/JobManager.h"
#include "utils/Logger.h"
#include "utils/config_manager.h"
#include "metrics/PrometheusExporter.h"
#include "runtime/SystemMonitor.h"
#include "runtime/Profiler.h"
#include "utils/StatusExporter.h"
#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#undef ERROR
#undef INFO
#endif

using std::chrono::steady_clock;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

// --- Variables globales de control ---
static std::atomic<bool> g_running{true};
static std::atomic<bool> g_reload_config{false};
static std::mutex console_mutex;
static std::condition_variable_any console_cv;

// --- Checkpoint: Estado persistente ---
const std::string CHECKPOINT_FILE = "checkpoint/miner_status.json";
void saveCheckpoint(const MinerStatus& status) {
    std::filesystem::create_directories("checkpoint");
    std::ofstream out(CHECKPOINT_FILE);
    out << status.toJson().dump(4) << std::endl;
}
MinerStatus loadCheckpoint() {
    MinerStatus status;
    std::ifstream in(CHECKPOINT_FILE);
    if (in) {
        nlohmann::json j;
        in >> j;
        status.fromJson(j);
    }
    return status;
}

// --- Señales UNIX/Windows ---
void signalHandler(int signal) {
    Logger::info("🛑 Señal {} recibida. Iniciando apagado seguro...", signal);
    g_running = false;
}
void sighupHandler(int signal) {
    Logger::info("🔁 Señal SIGHUP recibida. Hot reload de configuración...");
    g_reload_config = true;
}

// --- Consola interactiva ---
void consoleLoop() {
    Logger::info("🎛️  Consola: [q] para salir | [r] recargar configuración");
    while (g_running) {
        char ch = std::cin.get();
        if (ch == 'q' || ch == 'Q') { g_running = false; }
        if (ch == 'r' || ch == 'R') { g_reload_config = true; }
    }
}

// --- Validación de endpoint IA/Pool ---
bool validateEndpoint(const std::string& url) {
    constexpr const char* valid_prefixes[] = {"http://", "https://", "zmq+tcp://"};
    for (const auto& prefix : valid_prefixes) {
        if (url.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    try {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        std::signal(SIGABRT, signalHandler);
#ifdef SIGHUP
        std::signal(SIGHUP, sighupHandler);
#endif

        Logger::info("🚀 Inicializando ZARTRUX Miner v{}", "3.0.0 PRODUCTION");

        // --- Hot reload config ---
        ConfigManager config("config/miner_config.json");
        if (!config.load()) {
            Logger::critical("❌ Fallo crítico: No se pudo cargar la configuración");
            return EXIT_FAILURE;
        }
        auto backup_config = config;

        Logger::info("💻 Recursos del sistema:");
        Logger::info("   - Núcleos lógicos: {}", std::thread::hardware_concurrency());
        auto sysData = SystemMonitor::getSystemData();
        Logger::info("   - Memoria total: {:.2f} GB", sysData.ram_total);

        // --- Pool/IA ---
        auto& jobManager = JobManager::instance();
        const auto ia_endpoint = config.get<std::string>("ia.endpoint", "http://127.0.0.1:4444");
        if (!validateEndpoint(ia_endpoint)) {
            Logger::error("🔌 Endpoint IA inválido: {}", ia_endpoint);
            return EXIT_FAILURE;
        }
        jobManager.setIAEndpoint(ia_endpoint);
        Logger::info("🧠 Conectado a IA en: {}", ia_endpoint);

        // --- Configuración de minería ---
        MinerCore::MiningConfig miningConfig;
        miningConfig.threadCount = config.get<unsigned>("mining.threads", std::thread::hardware_concurrency());
        miningConfig.mode = config.get<std::string>("mining.mode", "Pool");
        miningConfig.seed = config.get<std::string>("mining.seed", std::nullopt);

        // --- Inicialización minero ---
        MinerCore miner(jobManager, miningConfig.threadCount);
        if (!miner.initialize(miningConfig)) {
            Logger::critical("❌ Fallo en la inicialización del núcleo de minería");
            return EXIT_FAILURE;
        }

        // --- Métricas Prometheus ---
        const auto metrics_enabled = config.get<bool>("metrics.enabled", true);
        if (metrics_enabled) {
            PrometheusExporter::getInstance().initialize(
                config.get<std::string>("metrics.endpoint", "0.0.0.0:9100")
            );
            Logger::info("📊 Métricas Prometheus habilitadas en puerto 9100");
        }

        // --- Cargar checkpoint si existe ---
        if (std::filesystem::exists(CHECKPOINT_FILE)) {
            Logger::info("🔄 Restaurando estado desde checkpoint...");
            MinerStatus prev = loadCheckpoint();
            miner.restoreFromStatus(prev); // implementa en MinerCore si aún no está
        }

        // --- Hilo consola interactiva ---
        std::thread consoleThread(consoleLoop);

        // --- Bucle principal ---
        Logger::info("⛏️ Iniciando proceso de minería...");
        miner.startMining();

        const auto cycle_delay = config.get<unsigned>("performance.cycle_delay_ms", 50);
        const auto heartbeat_interval = config.get<unsigned>("monitoring.heartbeat_interval", 5000);

        auto last_heartbeat = steady_clock::now();
        auto last_stats = steady_clock::now();
        auto last_export = steady_clock::now();
        auto last_checkpoint = steady_clock::now();
        zartrux::runtime::Profiler::PerformanceMonitor perfMon(60); // CAMBIO: aquí creamos el monitor
        float last_hashrate = 0.0f;
        std::vector<float> hashrate_history(6, 0.0f);

        while (g_running) {
            const auto start_time = steady_clock::now();

            // Hot-reload config
            if (g_reload_config) {
                Logger::info("🔁 Recargando configuración desde disco...");
                if (config.load()) {
                    miningConfig.threadCount = config.get<unsigned>("mining.threads", std::thread::hardware_concurrency());
                    miningConfig.mode = config.get<std::string>("mining.mode", "Pool");
                    miningConfig.seed = config.get<std::string>("mining.seed", std::nullopt);
                    miner.reloadConfig(miningConfig);
                    Logger::info("✅ Configuración recargada correctamente.");
                } else {
                    Logger::error("❌ Error al recargar configuración.");
                }
                g_reload_config = false;
            }

            perfMon.update(miner.getWorkerStats()); // CAMBIO: Actualiza monitor

            // Estadísticas consola cada 10s
            if (steady_clock::now() - last_stats > 10s) {
                auto sysData = SystemMonitor::getSystemData();
                Logger::debug("📈 Estadísticas: Hashes/s: {:.2f} | Memoria: {:.2f}% | Temp: {:.1f}°C",
                    perfMon.getHashrate(), // CAMBIO
                    (sysData.ram_usage / sysData.ram_total) * 100.0f,
                    sysData.cpu_temp);
                last_stats = steady_clock::now();
            }

            // Heartbeat Prometheus
            if (metrics_enabled && steady_clock::now() - last_heartbeat > milliseconds(heartbeat_interval)) {
                miner.updateMetrics();
                PrometheusExporter::getInstance().pushMetrics();
                last_heartbeat = steady_clock::now();
            }

            // Exportar estado backend/web cada 2s
            if (steady_clock::now() - last_export > 2s) {
                auto sysData = SystemMonitor::getSystemData();
                MinerStatus status;
                status.mining_active = miner.isMining();
                status.mining_seconds = miner.getMiningTime();
                status.active_threads = miner.getActiveThreads();
                status.total_threads = miner.getThreadCount();
                status.ram_usage = sysData.ram_usage;
                status.total_ram = sysData.ram_total;
                status.cpu_usage = sysData.cpu_usage;
                status.cpu_speed = sysData.cpu_speed;
                status.cpu_temp = sysData.cpu_temp;
                status.hashrate = perfMon.getHashrate(); // CAMBIO
                status.shares = miner.getAcceptedShares();
                status.difficulty = miner.getCurrentDifficulty();
                status.current_block = miner.getCurrentBlock();
                status.block_status = miner.getBlockStatus();
                status.temperature = miner.getTemperature();
                status.temp_status = miner.getTempStatus();
                status.mode = miner.getCurrentMode();

                // Tendencia de hashrate
                float current_hashrate = perfMon.getHashrate(); // CAMBIO
                if (last_hashrate > 0) {
                    float trend = ((current_hashrate - last_hashrate) / last_hashrate) * 100.0f;
                    char trend_str[32];
                    snprintf(trend_str, sizeof(trend_str), "%+.2f%%", trend);
                    status.hash_trend = trend_str;
                } else {
                    status.hash_trend = "+0.00%";
                }
                last_hashrate = current_hashrate;
                hashrate_history.erase(hashrate_history.begin());
                hashrate_history.push_back(current_hashrate);
                status.hashrate_history = hashrate_history;

                StatusExporter::exportStatus(status);
                last_export = steady_clock::now();

                // Checkpoint a disco cada 2s
                if (steady_clock::now() - last_checkpoint > 2s) {
                    saveCheckpoint(status);
                    last_checkpoint = steady_clock::now();
                }
            }

            // Control ciclo
            const auto elapsed = steady_clock::now() - start_time;
            if (elapsed < milliseconds(cycle_delay)) {
                std::this_thread::sleep_for(milliseconds(cycle_delay) - std::chrono::duration_cast<milliseconds>(elapsed));
            }
        }

        // Apagado seguro
        Logger::info("🛑 Iniciando secuencia de apagado...");
        miner.stopMining();
        PrometheusExporter::getInstance().shutdown();
        console_cv.notify_all();
        if (consoleThread.joinable()) consoleThread.join();
        Logger::info("✅ Minería detenida correctamente. Recursos liberados");
        Logger::info("👋 Sesión finalizada. Hasta pronto!");
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        Logger::critical("💥 ERROR CRÍTICO: {}", e.what());
        return EXIT_FAILURE;
    }
    catch (...) {
        Logger::critical("💥 ERROR DESCONOCIDO");
        return EXIT_FAILURE;
    }
}

void printSystemInfo() {
    auto info = zartrux::runtime::Profiler::getSystemInfo();
    std::cout << "CPU: " << info.cpuName << std::endl;
    std::cout << "Physical cores: " << info.physicalCores << std::endl;
    std::cout << "Logical cores: " << info.logicalCores << std::endl;
    std::cout << "L3 cache (MB): " << info.l3CacheSizeMB << std::endl;
    std::cout << "RAM (MB): " << info.totalRamMB << std::endl;
    std::cout << "CPU features:";
    for (auto feat : info.features)
        std::cout << " " << zartrux::runtime::Profiler::featureToString(feat);
    std::cout << std::endl;
}
