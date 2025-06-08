#ifndef ZARTRUX_MINING_MODE_MANAGER_H
#define ZARTRUX_MINING_MODE_MANAGER_H
#include "JobManager.h"
#include <mutex>
#include <string>
#include <stdexcept>
#include <atomic>

/**
 * Enum de modos de minería disponibles en el sistema.
 */
enum class MiningMode {
    SOLO,   ///< Modo autónomo sin conexión externa
    HYBRID, ///< Combinación de minería tradicional e IA
    POOL,   ///< Conexión a pools de minería tradicional
    IA      ///< Minería 100% dirigida por IA
};

/**
 * Convierte un modo de minería a string legible.
 * @param mode Modo a convertir
 * @return Representación en string del modo
 */
std::string modeToString(MiningMode mode);

/**
 * Convierte string a modo de minería
 * @param modeStr String a convertir
 * @return Modo de minería equivalente
 * @throws std::invalid_argument si el string no es válido
 */
MiningMode stringToMode(const std::string& modeStr);

/**
 * Excepción lanzada cuando una transición entre modos es inválida.
 */
class MiningModeException : public std::runtime_error {
public:
    MiningModeException(MiningMode from, MiningMode to);
};

/**
 * Clase singleton que gestiona el modo de operación del sistema.
 */
class MiningModeManager {
public:
    MiningModeManager(const MiningModeManager&) = delete;
    MiningModeManager& operator=(const MiningModeManager&) = delete;

    static MiningModeManager& getInstance();

    void initialize();

    void setMode(MiningMode newMode);

    MiningMode getCurrentMode() const;

    bool canTransitionTo(MiningMode newMode) const;

private:
    MiningModeManager();

    void applyModeResources();
    void saveToConfig();
    void loadFromConfig();

    mutable std::mutex m_mutex;
    std::atomic<MiningMode> m_currentMode;
    MiningMode m_previousMode;
};

#endif // ZARTRUX_MINING_MODE_MANAGER_H
