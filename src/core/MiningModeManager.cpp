#include "MiningModeManager.h"
#include "PoolDispatcher.h"
#include "ia/IAReceiver.h"
#include "JobManager.h"
#include "utils/config_manager.h"
#include "utils/Logger.h"
#include "runtime/SystemMonitor.h"
#include "runtime/PowerSafe.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <mutex>
#include <string>
#include <utility>
#include <chrono>
#include <thread>

std::string modeToString(MiningMode mode) {
    switch (mode) {
        case MiningMode::SOLO: return "solo";
        case MiningMode::POOL: return "pool";
        case MiningMode::HYBRID: return "hybrid";
        case MiningMode::IA: return "ia";
        default: return "unknown";
    }
}

MiningMode stringToMode(const std::string& modeStr) {
    if (modeStr == "solo") return MiningMode::SOLO;
    if (modeStr == "pool") return MiningMode::POOL;
    if (modeStr == "hybrid") return MiningMode::HYBRID;
    if (modeStr == "ia") return MiningMode::IA;
    throw std::invalid_argument("Modo desconocido: " + modeStr);
}

MiningModeException::MiningModeException(MiningMode from, MiningMode to)
: std::runtime_error(
    "Transición de modo no permitida: " + modeToString(from) +
    " → " + modeToString(to)
) {}

// --- Singleton ---

MiningModeManager& MiningModeManager::getInstance() {
    static MiningModeManager instance;
    return instance;
}

MiningModeManager::MiningModeManager() {
    m_currentMode = MiningMode::SOLO; // Default seguro
    m_previousMode = MiningMode::SOLO;
}

void MiningModeManager::initialize() {
    loadFromConfig();
}

void MiningModeManager::loadFromConfig() {
    std::unique_lock<std::mutex> lock(m_mutex);
    try {
        std::string modeStr = ConfigManager::getString("mining_mode", "solo");
        m_currentMode = stringToMode(modeStr);
         Logger::info("MiningModeManager", "Modo cargado desde config: {}", modeToString(m_currentMode));
        applyModeResources();
    } catch (const std::exception& e) {
         Logger::error("MiningModeManager", "Error cargando modo desde config: {}", e.what());
        m_currentMode = MiningMode::SOLO; // Fallback seguro
    }
}

void MiningModeManager::saveToConfig() {
   try {
        auto& cfg = ConfigManager::getInstance();
        cfg.set("mining_mode", modeToString(m_currentMode));
        cfg.save();
    } catch (const std::exception& e) {
        Logger::error("MiningModeManager", "Error guardando modo: {}", e.what());
    }
}

bool MiningModeManager::canTransitionTo(MiningMode newMode) const {
    // Comprobación: No se puede pasar a POOL sin pools activos
    if (newMode == MiningMode::POOL && !PoolDispatcher::getInstance().hasActivePools())
        return false;

    // Comprobación: No se puede activar IA o HYBRID si la IA no está disponible
    if ((newMode == MiningMode::HYBRID || newMode == MiningMode::IA) &&
        !IAReceiver::getInstance().isAvailable())
        return false;

    // Restricción: Cambiar IA→POOL solo permitida con reinicio
    if (m_currentMode == MiningMode::IA && newMode == MiningMode::POOL) {
        Logger::warn("MiningModeManager", "Transición IA → POOL requiere reinicio del sistema.");
        return false;
    }

       // Validación adicional: No cambiar si la temperatura de CPU supera los 92ºC
    if (SystemMonitor::getSystemData().cpu_temp > 92.0) {
        Logger::error("[MiningModeManager] Temperatura demasiado alta para cambio de modo.");
        return false;
    }

    // Validación: Si la energía está en modo crítico, solo se permite SOLO o IA
    if (PowerSafe::getEnergyState() == PowerSafe::CRITICAL) {
        if (!(newMode == MiningMode::SOLO || newMode == MiningMode::IA)) {
            Logger::warn("MiningModeManager", "Sólo se permiten modos SOLO o IA en estado energético crítico.");
            return false;
        }
    }

    // Validación: Puedes añadir aquí cualquier lógica para hardware, memoria, etc.
    // Ejemplo: limitar hilos para HYBRID en CPUs antiguas
    // if (newMode == MiningMode::HYBRID && SystemMonitor::getCPUCoreCount() < 4) return false;

    return true;
}

void MiningModeManager::setMode(MiningMode newMode) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (newMode == m_currentMode) {
         Logger::debug("MiningModeManager", "Modo ya activo: {}", modeToString(newMode));
        return;
    }

    if (!canTransitionTo(newMode)) {
        throw MiningModeException(m_currentMode, newMode);
    }

    m_previousMode = m_currentMode;
    try {
        m_currentMode = newMode;
        lock.unlock(); // Liberar durante operaciones que pueden ser lentas (sin deadlock)
        applyModeResources();
        lock.lock();
        saveToConfig();
       Logger::info("MiningModeManager", "Cambio de modo: {} → {}", modeToString(m_previousMode), modeToString(newMode));
                     " → " + modeToString(newMode));
    } catch (const std::exception& e) {
      Logger::error("MiningModeManager", "Fallo al cambiar de modo: {}", e.what());
        Logger::error("[MiningModeManager] Fallo al cambiar de modo: " + std::string(e.what()));
        throw;
    }
}

MiningMode MiningModeManager::getCurrentMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentMode;
}

void MiningModeManager::applyModeResources() {
    MiningMode mode = m_currentMode;
    auto& pool = PoolDispatcher::getInstance();
    auto& ia = IAReceiver::getInstance();
    auto& jobs = JobManager::getInstance();

    try {
        switch (mode) {
            case MiningMode::SOLO:
                pool.disable();
                ia.deactivate();
                jobs.setAIContribution(0.0f);
                break;
            case MiningMode::POOL:
                pool.enable();
                ia.deactivate();
                jobs.setAIContribution(0.0f);
                break;
            case MiningMode::IA:
                pool.disable();
                ia.activate();
                jobs.setAIContribution(1.0f);
                break;
            case MiningMode::HYBRID: {
                pool.enable();
                ia.activate();
                float contribution = ConfigManager::getFloat("ia_contribution", 0.5f);
                if (contribution < 0.0f || contribution > 1.0f) contribution = 0.5f;
                jobs.setAIContribution(contribution);
                break;
            }
            default:
                throw std::runtime_error("Modo de minería desconocido en applyModeResources()");
        }
        // 🔥 Exportar estado a backend web JSON para consola HTML (monitoring zarbackend)
        // StatusExporter::export("mining_mode", modeToString(mode));
        // StatusExporter::export("cpu_temp", SystemMonitor::getCPUTemp());
        // StatusExporter::export("energy", PowerSafe::getEnergyStateString());
        // StatusExporter::flush();
        // Todo lo anterior sería para que zarbackend/server.py lo consuma en vivo
    } catch (const std::exception& ex) {
        Logger::critical("MiningModeManager", "Error al aplicar recursos: {}", ex.what());
        throw;
    }
}
