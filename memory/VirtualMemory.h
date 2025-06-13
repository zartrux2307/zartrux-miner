#pragma once

#include <cstddef>

namespace zartrux {

/**
 * @class VirtualMemory
 * @brief Proporciona una interfaz de bajo nivel para gestionar la memoria virtual.
 *
 * Esta clase abstrae las llamadas específicas del sistema operativo (Windows, Linux)
 * para reservar, liberar y cambiar los permisos de páginas de memoria.
 */
class VirtualMemory {
public:
    /**
     * @brief Reserva memoria que puede ser marcada como ejecutable (para JIT).
     * @param bytes El tamaño en bytes a reservar.
     * @param hugePages Si se deben intentar usar páginas grandes (huge pages).
     * @return Un puntero a la memoria reservada, o nullptr si falla.
     */
    static void* allocateExecutableMemory(size_t bytes, bool hugePages);

    /**
     * @brief Reserva memoria utilizando páginas grandes para mejorar el rendimiento.
     * @param bytes El tamaño en bytes a reservar.
     * @return Un puntero a la memoria reservada, o nullptr si falla.
     */
    static void* allocateLargePagesMemory(size_t bytes);

    /**
     * @brief Libera la memoria previamente reservada.
     * @param ptr Puntero a la memoria a liberar.
     * @param bytes El tamaño que fue reservado (necesario en sistemas POSIX).
     */
    static void freeLargePagesMemory(void* ptr, size_t bytes);

    /**
     * @brief Cambia la protección de una región de memoria a Lectura y Ejecución (RX).
     * @param ptr Puntero a la región de memoria.
     * @param bytes Tamaño de la región.
     */
    static void protectRX(void* ptr, size_t bytes);

    /**
     * @brief Cambia la protección de una región de memoria a Lectura y Escritura (RW).
     * @param ptr Puntero a la región de memoria.
     * @param bytes Tamaño de la región.
     */
    static void protectRW(void* ptr, size_t bytes);
};

} // namespace zartrux