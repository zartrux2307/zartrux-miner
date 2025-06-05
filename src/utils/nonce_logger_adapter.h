#pragma once

#include <string>
#include <mutex>
#include "Logger.h"

namespace pybind11 { class dict; }

class NonceLoggerAdapter {
public:
    explicit NonceLoggerAdapter(const std::string& context = "NonceLogger");
    void logPythonEvent(const std::string& level, const std::string& message, const pybind11::dict& extra);
    void logExportProgress(size_t processed, size_t total, const std::string& additionalInfo = "");
    void logFileOperation(const std::string& operation, const std::string& filename, bool success);
    void setNonceLoggingPrecision(int precision);
    int getNonceLoggingPrecision() const;
    static std::mutex& globalMutex();

private:
    std::string context_;
    int precision_ = 6;
    static std::mutex logger_mutex_;
    std::string formatNonceMessage(const std::string& base, const pybind11::dict& extra);
    static bool isFilenameSafe(const std::string& filename);
};
