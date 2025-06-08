cmake_minimum_required(VERSION 3.15)
project(zartrux_src LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opciones de compilación
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -march=native -fPIC)
endif()

find_package(Threads REQUIRED)

# Recopilar fuentes por módulo
file(GLOB_RECURSE CORE_SRC "core/*.cpp")
file(GLOB_RECURSE NETWORK_SRC "network/*.cpp")
file(GLOB_RECURSE MEMORY_SRC "memory/*.cpp")
file(GLOB_RECURSE RUNTIME_SRC "runtime/*.cpp")
file(GLOB_RECURSE UTILS_SRC "utils/*.cpp")
file(GLOB_RECURSE SECURITY_SRC "security/*.cpp")
file(GLOB_RECURSE THREADS_SRC "core/threads/*.cpp")

# Recopilar fuentes de crypto
file(GLOB_RECURSE CRYPTO_SRC
    "crypto/rx/*.cpp"
    "crypto/randomx/*.cpp"
    "crypto/randomx/*.c"
    "crypto/randomx/blake2/*.c"
    "crypto/randomx/blake2/avx2/*.c"
)
# Añadir explícitamente los archivos de ensamblador
set(CRYPTO_ASM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_a64_static.S
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_x86_static.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_x86_static.S
)


# Librería interna principal
add_library(zartrux_core_libs STATIC
    ${CORE_SRC}
    ${NETWORK_SRC}
    ${MEMORY_SRC}
    ${RUNTIME_SRC}
    ${UTILS_SRC}
    ${SECURITY_SRC}
    ${THREADS_SRC}
    ${CRYPTO_SRC}
    ${CRYPTO_ASM_SRC}
)

target_include_directories(zartrux_core_libs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/threads
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/security
    # Directorios de crypto añadidos
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rx
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/blake2
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/blake2/avx2
)

target_link_libraries(zartrux_core_libs PUBLIC Threads::Threads)

# Ejecutable principal
add_executable(zartrux_miner zarmain.cpp)
target_link_libraries(zartrux_miner PRIVATE zartrux_core_libs)

# Instalación
install(TARGETS zartrux_core_libs zartrux_miner
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

message(STATUS "✅ src/ CMake configurado correctamente con todos los módulos integrados.")