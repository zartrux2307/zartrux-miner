#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    running_.store(true);
    workerThread_ = std::thread(&Logger::processQueue, this);
}

Logger::~Logger() noexcept {
    running_.store(false);
    cv_.notify_all();
    if (workerThread_.joinable()) workerThread_.join();
    if (logFile_.is_open()) logFile_.close();
}

void Logger::init(const std::string& logPath, bool colorConsole, size_t rotateEveryN) {
    Logger& inst = Logger::instance();
    std::lock_guard<std::mutex> lock(inst.queueMutex_);
    if (inst.logFile_.is_open()) inst.logFile_.close();
    inst.logPath_ = logPath;
    inst.toConsole_ = true;
    inst.colorConsole_ = colorConsole;
    inst.rotateEveryN_ = rotateEveryN;
    inst.logCounter_ = 0;
    if (!logPath.empty()) {
        inst.logFile_.open(logPath, std::ios::app);
        if (!inst.logFile_.is_open()) {
            std::cerr << "No se pudo abrir el archivo de log: " << logPath << std::endl;
        }
    }
}

void Logger::log(Level level, const std::string& component, const std::string& message) {
    Logger& inst = Logger::instance();
    {
        std::lock_guard<std::mutex> lock(inst.queueMutex_);
        inst.queue_.push({ level, component, message });
    }
    inst.cv_.notify_one();
}

void Logger::debug(const std::string& component, const std::string& message) {
    log(Level::DEBUG, component, message);
}
void Logger::info(const std::string& component, const std::string& message) {
    log(Level::INFO, component, message);
}
void Logger::warn(const std::string& component, const std::string& message) {
    log(Level::WARNING, component, message);
}
void Logger::error(const std::string& component, const std::string& message) {
    log(Level::ERROR_LEVEL, component, message);
}
void Logger::critical(const std::string& component, const std::string& message) {
    log(Level::CRITICAL, component, message);
}
void Logger::logError(const std::string& component, const std::string& message) {
    log(Level::ERROR_LEVEL, component, message);
}

void Logger::error(const std::string& component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logFormatted(Level::ERROR_LEVEL, component, format, args);
    va_end(args);
}
void Logger::warn(const std::string& component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logFormatted(Level::WARNING, component, format, args);
    va_end(args);
}
void Logger::info(const std::string& component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logFormatted(Level::INFO, component, format, args);
    va_end(args);
}
void Logger::debug(const std::string& component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logFormatted(Level::DEBUG, component, format, args);
    va_end(args);
}

void Logger::processQueue() {
    while (running_.load() || !queue_.empty()) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || !running_.load(); });
        while (!queue_.empty()) {
            LogEntry entry = std::move(queue_.front());
            queue_.pop();
            lock.unlock();

            std::ostringstream oss;
            oss << "[" << getTimeString() << "] "
                << "[" << levelToString(entry.level) << "] "
                << "[" << entry.component << "] "
                << entry.message << std::endl;

            writeLogEntry(oss.str(), entry.level);
            logCounter_++;
            rotateLogFileIfNeeded();

            lock.lock();
        }
    }
}

void Logger::writeLogEntry(const std::string& entry, Level level) {
    if (toConsole_) {
        if (colorConsole_) {
#ifdef _WIN32
            // Para colores reales, puedes añadir SetConsoleTextAttribute aquí si lo deseas
#else
            if (level == Level::ERROR_LEVEL || level == Level::CRITICAL)
                std::cout << "\033[31m"; // Rojo
            else if (level == Level::WARNING)
                std::cout << "\033[33m"; // Amarillo
            else if (level == Level::INFO)
                std::cout << "\033[32m"; // Verde
            else if (level == Level::DEBUG)
                std::cout << "\033[36m"; // Cyan
#endif
        }
        std::cout << entry;
        if (colorConsole_) std::cout << "\033[0m";
    }
    if (logFile_.is_open()) {
        logFile_ << entry;
        logFile_.flush();
    }
}

std::string Logger::getTimeString() const {
    std::time_t now = std::time(nullptr);
    std::tm timeInfo{};
#ifdef _WIN32
    localtime_s(&timeInfo, &now);
#else
    localtime_r(&now, &timeInfo);
#endif
    std::ostringstream ss;
    ss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::levelToString(Level level) const {
    switch (level) {
        case Level::DEBUG:    return "DEBUG";
        case Level::INFO:     return "INFO";
        case Level::WARNING:  return "WARN";
        case Level::ERROR_LEVEL:    return "ERROR";
        case Level::CRITICAL: return "CRITICAL";
        default:              return "UNKNOWN";
    }
}

void Logger::rotateLogFileIfNeeded() {
    if (rotateEveryN_ == 0 || logPath_.empty()) return;
    if (logCounter_ >= rotateEveryN_) {
        logFile_.close();
        std::string rotatedName = logPath_ + "." + getTimeString();
        std::replace(rotatedName.begin(), rotatedName.end(), ' ', '_');
        std::replace(rotatedName.begin(), rotatedName.end(), ':', '_');
        std::filesystem::rename(logPath_, rotatedName);
        logFile_.open(logPath_, std::ios::app);
        logCounter_ = 0;
        if (!logFile_.is_open()) {
            std::cerr << "No se pudo reabrir archivo de log tras rotación." << std::endl;
        }
    }
}

void Logger::logFormatted(Level level, const std::string& component, const char* format, va_list args) {
    constexpr size_t bufsize = 1024;
    char buffer[bufsize];
    vsnprintf(buffer, bufsize, format, args);
    log(level, component, std::string(buffer));
}
