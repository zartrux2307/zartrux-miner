#include "StatusExporter.h"
#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <filesystem>
#include "Logger.h"
#include "config_manager.h"

std::mutex StatusExporter::status_mutex;

void StatusExporter::exportStatus(const MinerStatus& status) {
    std::lock_guard<std::mutex> lock(status_mutex);

    try {
        nlohmann::json j;
        j["status"] = status.mining_active ? "mining" : "inactive";
        j["mining_time"] = formatTime(status.mining_seconds);
        j["threads"] = std::to_string(status.active_threads) + "/" + std::to_string(status.total_threads);
        j["ram"] = std::to_string(status.ram_usage) + "/" + std::to_string(status.total_ram) + " GB";
        j["cpu_usage"] = std::to_string(status.cpu_usage) + "%";
        j["cpu_speed"] = std::to_string(status.cpu_speed) + " GHz";
        j["cpu_temp"] = std::to_string(status.cpu_temp) + "°C";
        j["hashrate"] = status.hashrate;
        j["shares"] = status.shares;
        j["difficulty"] = status.difficulty;
        j["block"] = status.current_block;
        j["block_status"] = status.block_status;
        j["temp"] = status.temperature;
        j["temp_status"] = status.temp_status;
        j["threads_progress"] = static_cast<int>((static_cast<float>(status.active_threads) / status.total_threads) * 100);
        j["ram_progress"] = static_cast<int>((status.ram_usage / status.total_ram) * 100);
        j["hash_trend"] = status.hash_trend;
        j["shares_trend"] = status.shares_trend;
        j["diff_trend"] = status.diff_trend;
        j["mode"] = status.mode;

        // Historial de hashrate rotativo (máx 120 muestras por defecto)
        std::vector<float> hist = status.hashrate_history;
        const size_t MAX_HISTORY = 120;
        if (hist.size() > MAX_HISTORY)
            hist = std::vector<float>(hist.end() - MAX_HISTORY, hist.end());
        j["hashrate_history"] = hist;

        // Ruta configurable con patrón singleton
        ConfigManager& config = ConfigManager::getInstance();
        std::string statusFilePath = config.getStatusFilePath();

        // Escritura atómica
        std::string tempFile = statusFilePath + ".tmp";
        std::ofstream file(tempFile);
        file << std::setw(4) << j << std::endl;
        file.close();
        std::rename(tempFile.c_str(), statusFilePath.c_str());

    } catch (const std::exception& e) {
        Logger::logError("StatusExporter", std::string("Error exporting status: ") + e.what());
    }
}

void StatusExporter::exportStatusJSON(size_t cpuQueue, size_t iaQueue,
                                      size_t validNonces,
                                      size_t processedCount) {
    std::lock_guard<std::mutex> lock(status_mutex);
    try {
        nlohmann::json j;
        j["cpu_queue"] = cpuQueue;
        j["ia_queue"] = iaQueue;
        j["valid_nonces"] = validNonces;
        j["processed_nonces"] = processedCount;

        std::filesystem::create_directories("zarbackend");
        const std::string path = "zarbackend/jobmanager_status.json";
        const std::string tmp = path + ".tmp";
        std::ofstream file(tmp);
        file << std::setw(4) << j << std::endl;
        file.close();
        std::rename(tmp.c_str(), path.c_str());
    } catch (const std::exception& e) {
        Logger::logError("StatusExporter",
                         std::string("Error exporting job status: ") + e.what());
    }
}


std::string StatusExporter::formatTime(long seconds) {
    long hours = seconds / 3600;
    long minutes = (seconds % 3600) / 60;
    long secs = seconds % 60;

    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02ld:%02ld:%02ld", hours, minutes, secs);
    return std::string(buffer);
}
