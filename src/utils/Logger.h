#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <ctime>
#include <iostream>
#include <cstdarg>

class Logger {
public:
    enum Level {
        DEBUG, INFO, WARN, ERROR, CRITICAL
    };

    // Log con componente y mensaje
    static void debug(const std::string& component, const std::string& message);
    static void info(const std::string& component, const std::string& message);
    static void warn(const std::string& component, const std::string& message);
    static void error(const std::string& component, const std::string& message);
    static void critical(const std::string& component, const std::string& message);

    // Log con printf style
    static void debug(const std::string& component, const char* fmt, ...);
    static void info(const std::string& component, const char* fmt, ...);
    static void warn(const std::string& component, const char* fmt, ...);
    static void error(const std::string& component, const char* fmt, ...);
    static void critical(const std::string& component, const char* fmt, ...);

    static void setLevel(Level lvl);
    static void setLogFile(const std::string& filename);

private:
    static void log(Level lvl, const std::string& component, const std::string& message);
    static std::string levelToString(Level lvl);
    static std::string currentDateTime();
    static std::ofstream logFile;
    static Level currentLevel;
    static std::mutex logMutex;
};
