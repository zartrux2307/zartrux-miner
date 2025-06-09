cmake_minimum_required(VERSION 3.20)

# 1. Definición del Proyecto
project(ZartruxMiner VERSION 1.0.0 LANGUAGES CXX C ASM)

# 2. Configuración del Compilador de C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Silenciamos advertencias comunes que se tratan como errores
    add_compile_options(/W4 /MP /permissive- /Zc:preprocessor /EHsc /utf-8 /wd4324 /wd4100 /wd4267 /wd4244)
    add_definitions(-D_UNICODE -DUNICODE -D_CRT_SECURE_NO_WARNINGS)
else() # GCC/Clang
    add_compile_options(-Wall -Wextra -Wpedantic -fPIC -pthread -Wno-unused-parameter)
endif()


# 3. --- BÚSQUEDA DE DEPENDENCIAS ---
message(STATUS "🔎 Buscando dependencias...")

# Para QT (instalación manual) - ¡IMPORTANTE!
# Le decimos a CMake dónde buscar tu instalación de Qt.
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.0/msvc2022_64" ${CMAKE_PREFIX_PATH})

# Para las demás librerías, usamos vcpkg
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "❌ Vcpkg toolchain (CMAKE_TOOLCHAIN_FILE) no está definido.")
endif()
message(STATUS "✅ Usando Vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")

find_package(Threads REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(cpr REQUIRED)
find_package(fmt REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(pybind11 REQUIRED)
message(STATUS "✅ Todas las dependencias principales han sido encontradas.")


# 4. Incluir el subdirectorio 'src' que contiene toda la lógica del minero
add_subdirectory(src)


# 5. Opciones de compilación avanzadas (como LTO)
option(ZARTRUX_ENABLE_LTO "Enable Link Time Optimization" ON)
if(ZARTRUX_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "🚀 LTO habilitado para compilaciones Release.")
    endif()
endif()

# 6. Instalación del ejecutable final
install(TARGETS zartrux-miner RUNTIME DESTINATION bin)

message(STATUS "✨ Configuración de ZartruxMiner finalizada.")