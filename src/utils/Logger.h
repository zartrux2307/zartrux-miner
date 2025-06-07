#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <cstdio>
#include <sstream>
#include <cstdarg>

class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR_LEVEL,
        CRITICAL
    };

    static void init(const std::string& logPath, bool colorConsole = true, size_t rotateEveryN = 50000);
    static void log(Level level, const std::string& component, const std::string& message);

    static void debug(const std::string& component, const std::string& message);
    static void info(const std::string& component, const std::string& message);
    static void warn(const std::string& component, const std::string& message);
    static void error(const std::string& component, const std::string& message);
    static void critical(const std::string& component, const std::string& message);
    static void logError(const std::string& component, const std::string& message);

    // Formatos variádicos
    static void error(const std::string& component, const char* format, ...);
    static void warn(const std::string& component, const char* format, ...);
    static void info(const std::string& component, const char* format, ...);
    static void debug(const std::string& component, const char* format, ...);

    // Simples (sin componente)
    static void debug(const std::string& message) { debug("General", message); }
    static void info(const std::string& message)  { info("General", message); }
    static void warn(const std::string& message)  { warn("General", message); }
    static void error(const std::string& message) { error("General", message); }
    static void critical(const std::string& message) { critical("General", message); }
    static void logError(const std::string& message) { logError("General", message); }

    // Para cualquier tipo
    template <typename T>
    static void warn(const std::string& component, T value) {
        std::ostringstream oss;
        oss << value;
        log(Level::WARNING, component, oss.str());
    }
    template <typename T>
    static void info(const std::string& component, T value) {
        std::ostringstream oss;
        oss << value;
        log(Level::INFO, component, oss.str());
    }
    template <typename T>
    static void error(const std::string& component, T value) {
        std::ostringstream oss;
        oss << value;
        log(Level::ERROR_LEVEL, component, oss.str());
    }

    static void logFormatted(Level level, const std::string& component, const char* format, va_list args);

    static Logger& instance();
    ~Logger() noexcept;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();

    struct LogEntry {
        Level level;
        std::string component;
        std::string message;
    };

    void processQueue();
    void writeLogEntry(const std::string& entry, Level level);
    std::string getTimeString() const;
    std::string levelToString(Level level) const;
    void rotateLogFileIfNeeded();

    std::string logPath_;
    bool toConsole_ = true;
    bool colorConsole_ = true;
    size_t logCounter_ = 0;
    size_t rotateEveryN_ = 50000;
    std::ofstream logFile_;
    std::queue<LogEntry> queue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::thread workerThread_;
    std::atomic<bool> running_{true};
};
