#include "Logger.h"

#include <iomanip>
#include <chrono>
#include <vector>

std::ofstream Logger::logFile;
Logger::Level Logger::currentLevel = Logger::LogDebug;
std::mutex Logger::logMutex;

void Logger::init(const std::string& file, bool /*colorConsole*/, size_t /*rotateEveryN*/) {
    if (!file.empty())
        setLogFile(file);
}


// ---- Core log function ----
void Logger::log(Level lvl, const std::string& component, const std::string& message) {
    if (lvl < currentLevel) return;

    std::ostringstream oss;
    oss << "[" << currentDateTime() << "] ";
    oss << "[" << levelToString(lvl) << "] ";
    oss << "[" << component << "] ";
    oss << message << std::endl;

    std::lock_guard<std::mutex> lock(logMutex);

    // Always log to console
    std::cout << oss.str();

    // If file logging enabled, log to file
    if (logFile.is_open())
        logFile << oss.str();
}

// ---- Basic log calls ----
void Logger::debug(const std::string& component, const std::string& message)    { log(LogDebug,    component, message); }
void Logger::info(const std::string& component, const std::string& message)     { log(LogInfo,     component, message); }
void Logger::warn(const std::string& component, const std::string& message)     { log(LogWarn,     component, message); }
void Logger::error(const std::string& component, const std::string& message)    { log(LogError,    component, message); }
void Logger::critical(const std::string& component, const std::string& message) { log(LogCritical, component, message); }

// ---- Printf-style log calls ----
void Logger::debug(const std::string& component, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    log(LogDebug, component, buffer);
}
void Logger::info(const std::string& component, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
     log(LogInfo, component, buffer);
}
void Logger::warn(const std::string& component, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
   log(LogWarn, component, buffer);
}
void Logger::error(const std::string& component, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    log(LogError, component, buffer);
}
void Logger::critical(const std::string& component, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    log(LogCritical, component, buffer);
}

void Logger::logError(const std::string& component, const std::string& message) {
    error(component, message);
}

// ---- Log file and level ----
void Logger::setLevel(Level lvl) { currentLevel = lvl; }

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) logFile.close();
    logFile.open(filename, std::ios::out | std::ios::app);
}

std::string Logger::levelToString(Level lvl) {
    switch (lvl) {
          case LogDebug:    return "DEBUG";
        case LogInfo:     return "INFO ";
        case LogWarn:     return "WARN ";
        case LogError:    return "ERROR";
        case LogCritical: return "CRIT!";
        default:       return "UNKWN";
    }
}

std::string Logger::currentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
#if defined(_WIN32)
    struct tm timeinfo;
    localtime_s(&timeinfo, &t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return buf;
#else
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
#endif
}
