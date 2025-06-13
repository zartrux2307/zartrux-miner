#include "SystemMonitor.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#include <filesystem>
#include <cstdio>
#endif

// ----------- [Estáticos] -----------
std::vector<std::function<void(const SystemData&)>> SystemMonitor::listeners;
std::mutex SystemMonitor::listenersMutex;
std::optional<SystemData> SystemMonitor::lastData;
std::chrono::system_clock::time_point SystemMonitor::lastFetch = std::chrono::system_clock::now();

nlohmann::json SystemData::toJson() const {
    return nlohmann::json{
        {"cpu_usage", cpu_usage},
        {"ram_total", ram_total},
        {"ram_usage", ram_usage},
        {"cpu_temp", cpu_temp},
        {"cpu_speed", cpu_speed},
        {"node_id", node_id},
        {"os_name", os_name},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count()}
    };
}

SystemData SystemMonitor::getSystemData(int minRefreshMs) {
    static std::mutex fetchMutex;
    std::lock_guard<std::mutex> lock(fetchMutex);

    auto now = std::chrono::system_clock::now();
    if (lastData && std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFetch).count() < minRefreshMs) {
        return *lastData;
    }
    SystemData data = {0};
#ifdef _WIN32
    // ---- WINDOWS IMPLEMENTACIÓN ----
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    data.ram_total = memInfo.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
    data.ram_usage = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);

    // CPU Usage con PDH
    static PDH_HQUERY cpuQuery;
    static PDH_HCOUNTER cpuTotal;
    static bool initialized = false;
    if (!initialized) {
        PdhOpenQuery(NULL, NULL, &cpuQuery);
        PdhAddCounter(cpuQuery, TEXT("\\Processor(_Total)\\% Processor Time"), NULL, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
        initialized = true;
        data.cpu_usage = 0.0;
    } else {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
        data.cpu_usage = counterVal.doubleValue;
    }
    data.cpu_temp = 0.0; // Implementar con WMI si se desea
    data.cpu_speed = 0.0; // Implementar con WMI si se desea

    data.node_id = getNodeId();
    data.os_name = getOSName();
    data.timestamp = std::chrono::system_clock::now();
#else
    // ---- LINUX IMPLEMENTACIÓN ----
    // CPU usage
    static uint64_t last_idle = 0, last_total = 0;
    std::ifstream stat_file("/proc/stat");
    std::string line;
    std::getline(stat_file, line);
    std::istringstream iss(line.substr(5));
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;

    if (last_total != 0) {
        uint64_t total_diff = total - last_total;
        uint64_t idle_diff = idle - last_idle;
        data.cpu_usage = (total_diff - idle_diff) * 100.0 / total_diff;
    } else {
        data.cpu_usage = 0.0;
    }
    last_total = total;
    last_idle = idle;

    // RAM
    struct sysinfo info;
    sysinfo(&info);
    data.ram_total = info.totalram * info.mem_unit / (1024.0 * 1024.0 * 1024.0);
    data.ram_usage = (info.totalram - info.freeram) * info.mem_unit / (1024.0 * 1024.0 * 1024.0);

    // Temperatura y frecuencia
    data.cpu_temp = getCpuTemperature();
    data.cpu_speed = getCpuSpeed();
    data.node_id = getNodeId();
    data.os_name = getOSName();
    data.timestamp = now;
#endif
    lastData = data;
    lastFetch = now;
    notifyListeners();
    return data;
}

void SystemMonitor::addListener(std::function<void(const SystemData&)> cb) {
    std::lock_guard<std::mutex> lock(listenersMutex);
    listeners.push_back(cb);
}
void SystemMonitor::notifyListeners() {
    if (!lastData) return;
    std::lock_guard<std::mutex> lock(listenersMutex);
    for (auto& cb : listeners) {
        try { cb(*lastData); } catch (...) {/* manejar error en callback */}
    }
}

std::string SystemMonitor::exportLatestJson() {
    auto data = getSystemData();
    return data.toJson().dump();
}

std::string SystemMonitor::getNodeId() {
#ifdef _WIN32
    // WIN: Use registry or computer name
    char name[256];
    DWORD size = sizeof(name);
    if (GetComputerNameA(name, &size)) {
        return std::string(name);
    }
    return "unknown_win";
#else
    // LINUX: MAC address as unique id or /etc/machine-id
    std::ifstream id_file("/etc/machine-id");
    std::string id;
    if (id_file && std::getline(id_file, id)) return id;
    return "unknown_linux";
#endif
}
std::string SystemMonitor::getOSName() {
#ifdef _WIN32
    return "Windows";
#else
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        return std::string(buffer.sysname) + " " + buffer.release;
    }
    return "Linux";
#endif
}

#ifdef __linux__
double SystemMonitor::getCpuTemperature() {
    try {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/thermal")) {
            if (entry.path().string().find("thermal_zone") != std::string::npos) {
                std::ifstream temp_file(entry.path() / "temp");
                if (temp_file) {
                    double temp;
                    temp_file >> temp;
                    return temp / 1000.0; // miligrados a grados
                }
            }
        }
    } catch (...) {}
    return 0.0;
}
double SystemMonitor::getCpuSpeed() {
    std::ifstream cpuinfo_file("/proc/cpuinfo");
    std::string line;
    double max_speed = 0.0;
    while (std::getline(cpuinfo_file, line)) {
        if (line.find("cpu MHz") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                try {
                    double speed = std::stod(line.substr(pos + 1)) / 1000.0;
                    if (speed > max_speed) max_speed = speed;
                } catch (...) {}
            }
        }
    }
    return max_speed;
}
#endif

