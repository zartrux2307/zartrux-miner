# --- RECOPILACIÓN DE FUENTES PARA LA LIBRERÍA C++ PURA ---
# Nota: "bridge/python_bindings.cpp" NO se incluye aquí.
file(GLOB_RECURSE ALL_SOURCES
    "core/*.cpp" "core/threads/*.cpp" "core/ia/*.cpp"
    "network/*.cpp"
    "memory/*.cpp"
    "runtime/*.cpp"
    "utils/*.cpp"
    "security/*.cpp"
    "arch/*.cpp" "arch/x8ag/*.cpp"
    "crypto/rx/*.cpp"
    "crypto/randomx/*.cpp"
    "crypto/randomx/*.c"
    "crypto/randomx/blake2/*.c"
    "crypto/argon2/lib/*.c"
)

set(ASM_SOURCES)
if(MSVC)
    list(APPEND ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crypto/randomx/jit_compiler_x86_static.asm")
endif()

# --- LIBRERÍA C++ PURA ---
# Esta librería ya no tiene ninguna relación con Python.
add_library(zartrux_libs STATIC
    ${ALL_SOURCES}
    ${ASM_SOURCES}
)
target_include_directories(zartrux_libs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "crypto/argon2/include"
)
target_link_libraries(zartrux_libs PUBLIC 
    Threads::Threads
    OpenSSL::SSL OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    cpr::cpr fmt::fmt
     yaml-cpp
    Qt::Core Qt::Widgets
)
# (Otras definiciones como ZARTRUX_FEATURE_ASM se quedan igual)


# --- MÓDULO DE PYTHON (EL PUENTE) ---
# Se crea el módulo de enlace (.pyd/.so) usando la función de pybind11.
pybind11_add_module(zartrux_engine
    bridge/python_bindings.cpp
)
# Se enlaza el "puente" con nuestra librería C++ principal.
target_link_libraries(zartrux_engine PRIVATE zartrux_libs)


# --- EJECUTABLE NATIVO C++ ---
add_executable(zartrux-miner zarmain.cpp)
target_link_libraries(zartrux-miner PRIVATE zartrux_libs)