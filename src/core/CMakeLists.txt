cmake_minimum_required(VERSION 3.15)
project(zartrux_core VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)

# Archivos fuente principales
add_library(zartrux_core STATIC
    MinerCore.cpp
    JobManager.cpp
    PoolDispatcher.cpp
    NonceValidator.cpp
    MiningModeManager.cpp
    hash.cpp
    SmartCache.cpp
    threads/WorkerThread.cpp
    ia/IAReceiver.cpp
)

add_library(zartrux::core ALIAS zartrux_core)

# Incluir directorios de encabezados propios y públicos
target_include_directories(zartrux_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/threads>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ia>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/core>
)

# Enlace a dependencias del proyecto y del sistema
target_link_libraries(zartrux_core PUBLIC
    zartrux::runtime        # Profiler, PowerSafe, etc.
    zartrux::arch           # Optimizaciones CPU/RandomX
    OpenSSL::SSL
    randomx                 # RandomX engine
    libzmq                  # ZeroMQ
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

# Opciones de compilación seguras y modernas
target_compile_features(zartrux_core PUBLIC cxx_std_20)
target_compile_options(zartrux_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O3>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /O2>
)

install(TARGETS zartrux_core
    EXPORT zartrux_targets  # <-- Esto debe permanecer
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


# Mensaje de confirmación
message(STATUS "✅ Módulo 'core' configurado y listo para producción.")
