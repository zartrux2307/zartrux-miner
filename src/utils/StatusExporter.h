#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "config_manager.h"

struct MinerStatus {
    bool mining_active;
    long mining_seconds;
    int active_threads;
    int total_threads;
    float ram_usage;       // GB en uso
    float total_ram;       // GB totales
    float cpu_usage;       // Porcentaje
    float cpu_speed;       // GHz
    float cpu_temp;        // Celsius
    float hashrate;        // KH/s
    int shares;
    float difficulty;
    std::string current_block;
    std::string block_status;
    float temperature;     // Temperatura general (puede ser CPU)
    std::string temp_status;
    std::string hash_trend;
    std::string shares_trend;
    std::string diff_trend;
    std::vector<float> hashrate_history; // Historial de hashrate para la gráfica
    std::string mode;       // Modo de minería: "Pool", "IA", "Hybrid"
};

class StatusExporter {
public:
    static void exportStatus(const MinerStatus& status);
    static void exportStatusJSON(size_t cpuQueue, size_t iaQueue,
                                 size_t validNonces,
                                 size_t processedCount);
    
private:
    static std::mutex status_mutex;
    static std::string formatTime(long seconds);
};
