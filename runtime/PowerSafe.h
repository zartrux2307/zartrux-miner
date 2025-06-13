#pragma once
#include <functional>
#include <memory>

namespace zartrux::runtime {

/// Modo de funcionamiento relacionado con el consumo de potencia.
enum class PowerMode {
    PERFORMANCE,  ///< Modo de alto rendimiento.
    BALANCED,     ///< Modo equilibrado entre rendimiento y consumo.
    POWER_SAVE    ///< Modo de ahorro de energía.
};

// Declaración adelantada de AdaptiveScheduler para evitar dependencias pesadas en esta interfaz.
class AdaptiveScheduler;

/**
 * @brief Interfaz para sistemas avanzados de gestión de potencia y térmica.
 *
 * Implementaciones concretas (por ejemplo, PowerSafeDefault) deberán ajustar parámetros del sistema
 * o limitar la cantidad de hilos mineros según condiciones dinámicas de potencia y temperatura.
 *
 * La interfaz permite:
 *  - Establecer límites de temperatura y potencia.
 *  - Ajustar el estado de potencia (por ejemplo, reducir el número de hilos) en función de lecturas de sensores.
 *  - Monitoreo continuo en tiempo real mediante funciones proporcionadas y un AdaptiveScheduler para balanceo dinámico.
 */
class PowerSafe {
public:
    virtual ~PowerSafe() = default;

    /**
     * @brief Configura el límite máximo de temperatura (en °C) antes de aplicar throttling o iniciar un apagado de emergencia.
     * @param celsius Valor de la temperatura límite.
     */
    virtual void setTemperatureLimit(double celsius) = 0;

    /**
     * @brief Define el modo de funcionamiento de potencia/rendimiento.
     * @param mode Modo deseado, ya sea PERFORMANCE, BALANCED o POWER_SAVE.
     */
    virtual void setPowerMode(PowerMode mode) = 0;

    /**
     * @brief Establece el límite de consumo de energía para el proceso de minería.
     * @param watts Consumo máximo permitido en vatios.
     */
    virtual void setPowerLimit(double watts) = 0;

    /**
     * @brief Ajusta el estado de potencia actual según las condiciones (por ejemplo, disminuye hilos de minería)
     *        en función de las lecturas internas de temperatura y consumo.
     *
     * La implementación deberá utilizar las métricas internas o sensores externos para realizar ajustes
     * necesarios y cumplir con los límites establecidos.
     */
    virtual void adjustPowerState() = 0;

    /**
     * @brief Inicia el monitoreo continuo de la temperatura y el consumo de energía, realizando ajustes en tiempo real.
     *
     * Las funciones de monitoreo proporcionadas deben retornar la temperatura actual (°C) y el consumo de energía (W).
     * El AdaptiveScheduler se utilizará para ajustar dinámicamente el número de hilos de minería según estas métricas.
     *
     * @param tempMonitor  Función que retorna la temperatura actual en °C.
     * @param powerMonitor Función que retorna el consumo de energía actual en vatios.
     * @param scheduler    Shared pointer al AdaptiveScheduler encargado de administrar la dinámica de hilos.
     */
    virtual void startMonitoring(std::function<double()> tempMonitor,
                                 std::function<double()> powerMonitor,
                                 std::shared_ptr<AdaptiveScheduler> scheduler) = 0;

    /**
     * @brief Detiene el monitoreo en segundo plano (si se encuentra en ejecución).
     */
    virtual void stopMonitoring() = 0;

    /**
     * @brief Indica si se ha activado una condición de apagado de emergencia (por ejemplo, debido a sobrecalentamiento).
     * @return True si existe una condición crítica, false en caso contrario.
     */
    virtual bool isEmergencyShutdown() const = 0;
};

} // namespace zartrux::runtime
