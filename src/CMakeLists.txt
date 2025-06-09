# Este CMakeLists.txt compila todo el código fuente del proyecto.

# --- RECOPILACIÓN DE FUENTES ---
file(GLOB_RECURSE ALL_SOURCES
    "core/*.cpp" "core/threads/*.cpp" "core/ia/*.cpp"
    "network/*.cpp"
    "memory/*.cpp"
    "runtime/*.cpp"
    "utils/*.cpp"
    "security/*.cpp"
    "arch/x8ag/*.cpp"
    "crypto/rx/*.cpp"
    "crypto/randomx/*.cpp"
    "crypto/randomx/*.c"
    "crypto/randomx/blake2/*.c"
    "crypto/argon2/lib/*.c"
)

# Fuentes de Ensamblador (ASM) - Excluyendo las que no son para la plataforma actual
set(ASM_SOURCES)
if(MSVC)
    list(APPEND ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_x86_static.asm")
else()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|AMD64|x86_64|X86_64")
        list(APPEND ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_x86_static.S")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|AARCH64")
        list(APPEND ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_a64_static.S")
    endif()
endif()

# --- LIBRERÍA ESTÁTICA ---
add_library(zartrux_libs STATIC
    ${ALL_SOURCES}
    ${ASM_SOURCES}
)

# --- INCLUSIÓN DE DIRECTORIOS ---
target_include_directories(zartrux_libs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
   "crypto/argon2/include"
)

# --- DEFINICIONES Y ENLAZADO ---
target_compile_definitions(zartrux_libs PRIVATE XMRIG_FEATURE_ASM=1)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|AMD64|x86_64|X86_64")
    target_compile_definitions(zartrux_libs PRIVATE XMRIG_64_BIT=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|AARCH64")
    target_compile_definitions(zartrux_libs PRIVATE XMRIG_ARM=8)
endif()

# Enlazamos las dependencias encontradas por el CMakeLists.txt raíz.
target_link_libraries(zartrux_libs PUBLIC 
    Threads::Threads
    Qt::Core
    Qt::Widgets
    nlohmann_json::nlohmann_json
    cpr::cpr
    fmt::fmt
    yaml-cpp::yaml-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
    pybind11::embed
)

# --- EJECUTABLE PRINCIPAL ---
add_executable(zartrux-miner zarmain.cpp)
target_link_libraries(zartrux-miner PRIVATE zartrux_libs)

message(STATUS "✅ Módulos de 'src' configurados y enlazados.")