cmake_minimum_required(VERSION 3.15)
project(zartrux_src)

# Estándar C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opciones de compilación comunes
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -march=native -fPIC)
endif()

# Recopilar todas las fuentes y headers por módulos
file(GLOB_RECURSE CORE_SRC "core/*.cpp")
file(GLOB_RECURSE CORE_HDR "core/*.h")

file(GLOB_RECURSE NETWORK_SRC "network/*.cpp")
file(GLOB_RECURSE NETWORK_HDR "network/*.h")

file(GLOB_RECURSE MEMORY_SRC "memory/*.cpp")
file(GLOB_RECURSE MEMORY_HDR "memory/*.h")

file(GLOB_RECURSE RUNTIME_SRC "runtime/*.cpp")
file(GLOB_RECURSE RUNTIME_HDR "runtime/*.h")

file(GLOB_RECURSE UTILS_SRC "utils/*.cpp")
file(GLOB_RECURSE UTILS_HDR "utils/*.h")

file(GLOB_RECURSE METRICS_SRC "metrics/*.cpp")
file(GLOB_RECURSE METRICS_HDR "metrics/*.h")

file(GLOB_RECURSE SECURITY_SRC "security/*.cpp")
file(GLOB_RECURSE SECURITY_HDR "security/*.h")



# Librería interna principal para ser enlazada por otros módulos
add_library(zartrux_core_libs STATIC
    ${CORE_SRC} ${CORE_HDR}
    ${NETWORK_SRC} ${NETWORK_HDR}
    ${MEMORY_SRC} ${MEMORY_HDR}
    ${RUNTIME_SRC} ${RUNTIME_HDR}
    ${UTILS_SRC} ${UTILS_HDR}
    ${METRICS_SRC} ${METRICS_HDR}
    ${SECURITY_SRC} ${SECURITY_HDR}

)

target_include_directories(zartrux_core_libs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/metrics
    ${CMAKE_CURRENT_SOURCE_DIR}/security
    ${CMAKE_CURRENT_SOURCE_DIR}/zarbackend
)

target_link_libraries(zartrux_core_libs PUBLIC Threads::Threads randomx)

# Opción: agregar ejecutable principal (si tienes zarmain.cpp)
add_executable(zartrux_miner zarmain.cpp)
target_link_libraries(zartrux_miner PRIVATE zartrux_core_libs)

# Exportaciones para instalación futura
install(TARGETS zartrux_core_libs zartrux_miner
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

message(STATUS "✅ src/ CMake configurado correctamente con todos los módulos integrados.")
