#pragma once

#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp> // Requiere nlohmann_json para serialización JSON

// ⚠️ SOLO para monitoreo/logs/minería Monero XMR. No usar para validación criptográfica.

struct SystemData {
    double cpu_usage;      // Porcentaje de uso de CPU
    double ram_total;      // GB totales
    double ram_usage;      // GB usados
    double cpu_temp;       // Celsius (0.0 si no disponible)
    double cpu_speed;      // GHz (0.0 si no disponible)
    std::string node_id;   // Identificador único de nodo/minero
    std::string os_name;   // Nombre de SO
    std::chrono::system_clock::time_point timestamp; // Marca de tiempo

    nlohmann::json toJson() const;  // Serializa la métrica a JSON
};

class SystemMonitor {
public:
    // Obtiene la métrica actual del sistema (anti-flood: mínimo cada X ms)
    static SystemData getSystemData(int minRefreshMs = 200);

    // Añade un listener/callback para métricas en tiempo real (dashboard/websocket/prometheus)
    static void addListener(std::function<void(const SystemData&)> cb);

    // Llama a todos los listeners con el último dato
    static void notifyListeners();

    // Exporta la última métrica como JSON
    static std::string exportLatestJson();

    // Para sistemas distribuidos: obtiene node_id único y OS
    static std::string getNodeId();
    static std::string getOSName();

private:
#ifdef __linux__
    static double getCpuTemperature();
    static double getCpuSpeed();
#endif
    static std::vector<std::function<void(const SystemData&)>> listeners;
    static std::mutex listenersMutex;
    static std::optional<SystemData> lastData;
    static std::chrono::system_clock::time_point lastFetch;
};
