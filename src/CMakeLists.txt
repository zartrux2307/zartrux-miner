# Versión unificada y corregida de src/CMakeLists.txt
# Este archivo gestiona todos los módulos del proyecto desde un único lugar.

cmake_minimum_required(VERSION 3.15)

# Habilitar C++, C y Ensamblador (MASM para MSVC, GAS para otros)
project(zartrux-miner LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opciones de compilación
if(MSVC)
    add_compile_options(/W4 /WX /MP) # /MP para compilación paralela
else()
    add_compile_options(-Wall -Wextra -Wpedantic -fPIC)
endif()

# --- RECOPILACIÓN DE TODAS LAS FUENTES DEL PROYECTO ---

file(GLOB_RECURSE CORE_SOURCES "core/*.cpp")
file(GLOB_RECURSE NETWORK_SOURCES "network/*.cpp")
file(GLOB_RECURSE MEMORY_SOURCES "memory/*.cpp")
file(GLOB_RECURSE RUNTIME_SOURCES "runtime/*.cpp")
file(GLOB_RECURSE UTILS_SOURCES "utils/*.cpp")
file(GLOB_RECURSE SECURITY_SOURCES "security/*.cpp")
file(GLOB_RECURSE ARCH_SOURCES "arch/x8ag/*.cpp")

# Fuentes de Crypto (C/C++)
file(GLOB_RECURSE CRYPTO_SOURCES
    "crypto/rx/*.cpp"
    "crypto/randomx/*.cpp"
    "crypto/randomx/*.c"
    "crypto/randomx/blake2/*.c"
    "crypto/randomx/blake2/avx2/*.c"
)

# Fuentes de Ensamblador (ASM)
set(ASM_SOURCES)
if(MSVC)
    # Para Visual Studio, usamos el archivo .asm
    list(APPEND ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_x86_static.asm")
else()
    # Para GCC/Clang, usamos los archivos .S
    list(APPEND ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_x86_static.S")
    list(APPEND ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_a64_static.S")
endif()


# --- LIBRERÍA ESTÁTICA PRINCIPAL ---
# Agrupamos todo el código del minero en una sola librería para modularidad.
add_library(zartrux_libs STATIC
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${MEMORY_SOURCES}
    ${RUNTIME_SOURCES}
    ${UTILS_SOURCES}
    ${SECURITY_SOURCES}
    ${ARCH_SOURCES}
    ${CRYPTO_SOURCES}
    ${ASM_SOURCES}
)

# --- INCLUSIÓN DE DIRECTORIOS DE CABECERAS ---
# Este es el paso clave para que el compilador encuentre todos los .h y .hpp
target_include_directories(zartrux_libs PUBLIC
    # Directorio 'src' base para permitir includes como "utils/Logger.h"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    
    # Directorios del proyecto
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/threads"
    "${CMAKE_CURRENT_SOURCE_DIR}/network"
    "${CMAKE_CURRENT_SOURCE_DIR}/memory"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils"
    "${CMAKE_CURRENT_SOURCE_DIR}/security"
    "${CMAKE_CURRENT_SOURCE_DIR}/arch"
    "${CMAKE_CURRENT_SOURCE_DIR}/arch/x8ag"
    
    # Directorios de crypto
    "${CMAKE_CURRENT_SOURCE_DIR}/crypto"
    "${CMAKE_CURRENT_SOURCE_DIR}/crypto/rx"
    "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx"
    "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/blake2"
    "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/blake2/avx2"

    # Directorio de 3rdparty si existe (ej. para argon2)
    # Asegúrate de que esta ruta sea correcta para tu proyecto
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/argon2/include" 
)

# --- DEFINICIONES DE PREPROCESADOR Y LIBRERÍAS ---
target_compile_definitions(zartrux_libs PRIVATE
    XMRIG_FEATURE_ASM=1
)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|AMD64|x86_64|X86_64")
    target_compile_definitions(zartrux_libs PRIVATE XMRIG_64_BIT=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|AARCH64")
    target_compile_definitions(zartrux_libs PRIVATE XMRIG_ARM=8)
endif()

# Encontrar y enlazar la librería de Hilos (Threads)
find_package(Threads REQUIRED)
target_link_libraries(zartrux_libs PUBLIC Threads::Threads)


# --- EJECUTABLE PRINCIPAL ---
add_executable(zartrux-miner zarmain.cpp)
target_link_libraries(zartrux-miner PRIVATE zartrux_libs)

# --- INSTALACIÓN ---
install(TARGETS zartrux_libs zartrux-miner
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

message(STATUS "✅ CMake de 'src' configurado correctamente. Todos los módulos integrados.")