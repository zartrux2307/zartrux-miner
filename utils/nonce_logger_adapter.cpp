#include "nonce_logger_adapter.h"
#include "Logger.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <thread>

namespace py = pybind11;

std::mutex NonceLoggerAdapter::logger_mutex_;

NonceLoggerAdapter::NonceLoggerAdapter(const std::string& context)
    : context_(context), precision_(6) {}

std::mutex& NonceLoggerAdapter::globalMutex() {
    return logger_mutex_;
}

// --- CORRECCIÓN: Implementación del método estático restaurado ---
void NonceLoggerAdapter::log_nonce(const std::string& nonce_hex, bool is_valid, size_t timestamp) {
    // Se crea una instancia para usar la infraestructura de logging
    NonceLoggerAdapter adapter("NonceValidator");
    py::dict extra;
    extra["hex"] = nonce_hex;
    extra["timestamp"] = timestamp;
    
    std::string message = is_valid ? "Nonce VÁLIDO encontrado" : "Nonce INVÁLIDO procesado";
    adapter.logPythonEvent(is_valid ? "INFO" : "DEBUG", message, extra);
}

void NonceLoggerAdapter::logPythonEvent(const std::string& level,
                                        const std::string& message,
                                        const pybind11::dict& extra) {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    Logger::Level logLevel;
    if (level == "DEBUG") logLevel = Logger::Level::DEBUG;
    else if (level == "INFO") logLevel = Logger::Level::INFO;
    else if (level == "WARNING") logLevel = Logger::Level::WARNING;
    else if (level == "ERROR") logLevel = Logger::Level::ERROR_LEVEL;
    else if (level == "CRITICAL") logLevel = Logger::Level::CRITICAL;
    else {
        Logger::log(Logger::Level::WARNING, context_,
                   "Nivel de log desconocido: " + level + ". Usando INFO por defecto.");
        logLevel = Logger::Level::INFO;
    }

    std::string formattedMsg = formatNonceMessage(message, extra);
    Logger::log(logLevel, context_, formattedMsg);
}

void NonceLoggerAdapter::logExportProgress(size_t processed,
                                           size_t total,
                                           const std::string& additionalInfo) {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    if (total == 0 || processed > total) {
        std::string error = "Datos inválidos en logExportProgress: processed="
                          + std::to_string(processed)
                          + " total=" + std::to_string(total);
        Logger::log(Logger::Level::ERROR_LEVEL, context_, error);
        return;
    }
    double percentage = (static_cast<double>(processed) / total) * 100.0;
    std::ostringstream oss;
    oss << "Progreso: " << processed << "/" << total
        << " (" << std::fixed << std::setprecision(2) << percentage << "%)";
    if (!additionalInfo.empty())
        oss << " - " << additionalInfo;
    Logger::log(Logger::Level::INFO, context_, oss.str());
}

void NonceLoggerAdapter::logFileOperation(const std::string& operation,
                                          const std::string& filename,
                                          bool success) {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    if (!isFilenameSafe(filename)) {
        Logger::log(Logger::Level::WARNING, context_,
                      "Nombre de archivo potencialmente inseguro: " + filename);
    }
    if (operation.empty() || filename.empty()) {
        Logger::log(Logger::Level::ERROR_LEVEL, context_,
                      "Operación o nombre de archivo vacío en logFileOperation.");
        return;
    }
    std::string message = operation + " " + filename + " - " + (success ? "ÉXITO" : "FALLO");
    Logger::log(Logger::Level::INFO, context_, message);
}

void NonceLoggerAdapter::setNonceLoggingPrecision(int precision) {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    if (precision < 0 || precision > 10) {
        Logger::log(Logger::Level::WARNING, context_,
                      "Precisión inválida: " + std::to_string(precision) + ". Usando valor por defecto (6).");
        precision_ = 6;
        return;
    }
    precision_ = precision;
}

int NonceLoggerAdapter::getNonceLoggingPrecision() const {
    return precision_;
}

std::string NonceLoggerAdapter::formatNonceMessage(const std::string& base, const pybind11::dict& extra) {
    std::ostringstream oss;
    oss << base;
    if (!extra.is_none() && py::len(extra) > 0) {
        oss << " [";
        bool first = true;
        for (auto& item : extra) {
            if (!first) oss << ", ";
            first = false;

            std::string key;
            try {
                key = py::cast<std::string>(item.first);
                key = std::regex_replace(key, std::regex(R"([\n\r\t\[\]=,])"), "_");
            } catch (...) {
                key = "<key-error>";
            }

            py::handle value = item.second;
            try {
                if (py::isinstance<py::float_>(value)) {
                    double num = py::cast<double>(value);
                    oss << key << "=" << std::fixed
                        << std::setprecision(precision_) << num;
                }
                else if (py::isinstance<py::int_>(value)) {
                    oss << key << "=" << py::cast<long>(value);
                }
                else if (py::isinstance<py::bool_>(value)) {
                    bool bval = py::cast<bool>(value);
                    oss << key << "=" << (bval ? "true" : "false");
                }
                else {
                    std::string strval = py::cast<std::string>(py::str(value));
                    strval = std::regex_replace(strval, std::regex(R"([\n\r\t\[\]=,])"), "_");
                    oss << key << "=" << strval;
                }
            } catch (const std::exception& e) {
                oss << key << "=<conversion-error>";
                Logger::log(Logger::Level::WARNING, context_,
                              "Error convirtiendo valor: " + key + ": " + e.what());
            }
        }
        oss << "]";
    }
    return oss.str();
}

bool NonceLoggerAdapter::isFilenameSafe(const std::string& filename) {
    static const std::regex safe_pattern(R"(^[A-Za-z0-9_\-\.]+$)");
    return std::regex_match(filename, safe_pattern);
}

// --- CORRECCIÓN: Bloque de bindings completo y corregido ---
PYBIND11_MODULE(nonce_adapter, m) {
    m.doc() = "Módulo de enlace para el adaptador de logging de Zartrux-Miner";

    py::class_<NonceLoggerAdapter>(m, "NonceLoggerAdapter", "Una clase que adapta el logger de C++ para su uso desde Python.")
        .def(py::init<const std::string&>(), "Constructor del adaptador",
             py::arg("context") = "NonceLogger")

        .def("log_python_event", &NonceLoggerAdapter::logPythonEvent, "Registra un evento genérico desde Python.",
             py::arg("level"), py::arg("message"), py::arg("extra") = py::dict())

        .def("log_export_progress", &NonceLoggerAdapter::logExportProgress, "Registra el progreso de una tarea de exportación.",
             py::arg("processed"), py::arg("total"), py::arg("additional_info") = "")

        .def("log_file_operation", &NonceLoggerAdapter::logFileOperation, "Registra una operación de archivo.",
             py::arg("operation"), py::arg("filename"), py::arg("success"))
             
        .def_property("nonce_precision", &NonceLoggerAdapter::getNonceLoggingPrecision, &NonceLoggerAdapter::setNonceLoggingPrecision,
                      "Controla la precisión decimal para los valores flotantes en los logs.")

        .def_static("log_nonce", &NonceLoggerAdapter::log_nonce, py::doc("Registra los detalles de un nonce validado."),
                    py::arg("nonce_hex"), py::arg("is_valid"), py::arg("timestamp"))
        
        .def_static("setup_global_logger", &Logger::init, py::doc("Configura el logger global de la aplicación."),
                    py::arg("log_path") = "");
}