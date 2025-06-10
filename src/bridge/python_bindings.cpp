/*
 * =================================================================================
 * ZARTUX-MINER PYTHON BRIDGE
 * =================================================================================
 * Este archivo es el único punto de contacto entre el núcleo C++ y Python.
 * Utiliza pybind11 para exponer clases y funciones clave, permitiendo un control
 * y monitoreo avanzados desde scripts de Python y el backend web.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

// Cabeceras de los módulos de zartrux-miner que vamos a exponer
#include "core/MinerCore.h"
#include "core/JobManager.h"
#include "core/NonceValidator.h"
#include "utils/Logger.h"
#include "utils/config_manager.h"
#include "runtime/Profiler.h"

// Usamos un alias para pybind11 para mayor legibilidad
namespace py = pybind11;

// Definición del Módulo de Python
// El primer argumento es el nombre con el que se importará en Python: import zartrux_engine
PYBIND11_MODULE(zartrux_engine, m) {
    m.doc() = "Módulo de enlace C++ para Zartrux Miner. Proporciona control sobre el núcleo del minero.";

    // ========================================================================
    // MÓDULO DE UTILIDADES (utils)
    // ========================================================================
    
    py::class_<Logger>(m, "Logger", "Clase para gestionar el sistema de logging.")
        .def_static("init", &Logger::init, "Inicializa el sistema de logging global.",
                    py::arg("log_path") = "");

    py::class_<ConfigManager>(m, "ConfigManager", "Gestor de configuración basado en JSON.")
        .def(py::init<const std::string&>(), py::arg("path"))
        .def("get_string", &ConfigManager::getString, "Obtiene un valor de tipo string.",
             py::arg("key"), py::arg("default_value") = "")
        .def("get_int", &ConfigManager::getInt, "Obtiene un valor de tipo entero.",
             py::arg("key"), py::arg("default_value") = 0)
        .def("get_bool", &ConfigManager::getBool, "Obtiene un valor de tipo booleano.",
             py::arg("key"), py::arg("default_value") = false);
    
    // ========================================================================
    // MÓDULO DE RUNTIME
    // ========================================================================

    py::class_<Profiler>(m, "Profiler", "Clase para la medición de rendimiento (profiling).")
        .def_static("start", &Profiler::start, "Inicia el cronómetro para una sección de código.", py::arg("name"))
        .def_static("stop", &Profiler::stop, "Detiene el cronómetro y registra la duración.", py::arg("name"))
        .def_static("print_report", &Profiler::printReport, "Imprime un informe detallado del rendimiento.");
        
    // ========================================================================
    // MÓDULO CORE (el cerebro del minero)
    // ========================================================================

    // Se exponen los enums para que puedan ser usados en Python de forma segura
    py::enum_<NonceValidator::Endianness>(m, "Endianness", "Define el orden de bytes (endianness) para un nonce.")
        .value("LITTLE", NonceValidator::Endianness::LITTLE, "Little Endian (ej. x86)")
        .value("BIG", NonceValidator::Endianness::BIG, "Big Endian")
        .export_values();

    // Se expone la configuración de minado de MinerCore
    py::class_<MinerCore::MiningConfig>(m, "MiningConfig", "Estructura de configuración para el MinerCore.")
        .def(py::init<>())
        .def_readwrite("seed", &MinerCore::MiningConfig::seed)
        .def_readwrite("thread_count", &MinerCore::MiningConfig::threadCount)
        .def_readwrite("mode", &MinerCore::MiningConfig::mode)
        .def_readwrite("nonce_position", &MinerCore::MiningConfig::noncePosition)
        .def_readwrite("nonce_size", &MinerCore::MiningConfig::nonceSize)
        .def_readwrite("nonce_endianness", &MinerCore::MiningConfig::nonceEndianness);

    // Se expone la clase principal MinerCore para control total desde Python
    // Se usa std::shared_ptr para una gestión de memoria segura entre C++ y Python
    py::class_<MinerCore, std::shared_ptr<MinerCore>>(m, "MinerCore", "Clase principal que controla el ciclo de vida del minero.")
        .def(py::init<std::shared_ptr<JobManager>, unsigned>(), py::arg("job_manager"), py::arg("thread_count") = 0)
        .def("initialize", &MinerCore::initialize, "Inicializa el núcleo del minero con una configuración.", py::arg("config"))
        .def("start", &MinerCore::start, "Inicia el proceso de minería en todos los hilos.")
        .def("stop", &MinerCore::stop, "Detiene el proceso de minería de forma segura.")
        .def("set_num_threads", &MinerCore::setNumThreads, "Ajusta el número de hilos de minería en tiempo real.", py::arg("count"))
        .def_property_readonly("is_mining", &MinerCore::isMining, "Devuelve true si el minero está activo.")
        .def_property_readonly("accepted_shares", &MinerCore::getAcceptedShares, "Devuelve el número de shares aceptados.")
        .def_property_readonly("active_threads", &MinerCore::getActiveThreads, "Devuelve el número de hilos actualmente activos.");

    // Se expone el JobManager para un control más granular de la IA
    py::class_<JobManager, std::shared_ptr<JobManager>>(m, "JobManager", "Gestiona los trabajos de minería y las colas de nonces.")
        .def_static("get_instance", &JobManager::getInstance, py::return_value_policy::reference, "Obtiene la instancia singleton del JobManager.")
        .def("set_ai_contribution", &JobManager::setAIContribution, "Establece el ratio de nonces de la IA (0.0 a 1.0).", py::arg("ratio"))
        .def("inject_ia_nonces", &JobManager::injectIANonces, "Inyecta una lista de nonces de la IA en la cola de trabajo.", py::arg("nonces"));
}