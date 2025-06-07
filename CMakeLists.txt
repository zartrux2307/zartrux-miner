cmake_minimum_required(VERSION 3.20)

project(ZartruxMiner LANGUAGES C CXX)
include(GNUInstallDirs)
option(ZARTRUX_ENABLE_IA "Enable IA modules" ON)

# Configuración C++ CORREGIDA
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # SOLUCIÓN: Usar solo /utf-8 y eliminar opciones conflictivas
    add_compile_options(/utf-8 /W4)
    add_definitions(-D_UNICODE -DUNICODE)
endif()
if(WIN32)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)
endif()

# VCPKG (mantener igual)
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "Directorio de vcpkg")
else()
    set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "Directorio de vcpkg")
endif()
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain file")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Triplet de vcpkg")

# ----------------- BLOQUE QT6 (mantener igual) -----------------
set(Qt6_DIR "C:/Qt/6.9.0/msvc2022_64/lib/cmake/Qt6")
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.9.0/msvc2022_64")

# Dependencias (mantener igual)
find_package(Qt6 REQUIRED COMPONENTS Core Quick Network Qml QmlModels Gui)
find_package(OpenSSL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

# Directorios de inclusión (mantener igual)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include
)

# Subdirectorios (mantener igual)
add_subdirectory(src/arch)
add_subdirectory(src/core)
add_subdirectory(src/utils)
add_subdirectory(src/runtime)
add_subdirectory(src/network)
add_subdirectory(src/memory)
add_subdirectory(src/security)
add_subdirectory(src/randomxzar)

if(ZARTRUX_ENABLE_IA)
    add_subdirectory(src/ia-modules)
endif()

# Ejecutable principal (mantener igual)
add_executable(zartrux-miner src/zarmain.cpp)

# Directorios de inclusión específicos (mantener igual)
target_include_directories(zartrux-miner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include
)

# Vinculación de bibliotecas (mantener igual)
target_link_libraries(zartrux-miner PRIVATE
    zartrux::arch
    zartrux::core
    zartrux::utils
    zartrux::runtime
    zartrux::network
    zartrux::memory
    zartrux::security
    randomxzar
    Qt6::Core 
    Qt6::Quick 
    Qt6::Network 
    Qt6::Qml 
    Qt6::QmlModels 
    Qt6::Gui
    OpenSSL::SSL 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    fmt::fmt
    cpr::cpr
    $<$<PLATFORM_ID:Windows>:Pdh.lib>
    libzmq
    yaml-cpp::yaml-cpp
)

# Resto del archivo (mantener igual hasta el final)
install(TARGETS zartrux-miner randomxzar
    EXPORT zartrux_targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zartrux
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/randomxzar/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zartrux/randomx
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/zarbackend/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/zartrux-miner/zarbackend
    USE_SOURCE_PERMISSIONS
    PATTERN "*.py" PERMISSIONS 
        OWNER_EXECUTE OWNER_WRITE OWNER_READ
        GROUP_EXECUTE GROUP_READ
        WORLD_EXECUTE WORLD_READ
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.log" EXCLUDE
)

install(EXPORT zartrux_targets
    FILE zartrux-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zartrux
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/zartrux-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config.cmake
    @ONLY
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zartrux
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

message(STATUS "✅ Configuración completa de ZartruxMiner")
message(STATUS "  - Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  - C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  - Soporte UTF-8: Habilitado")
message(STATUS "  - IA Modules: ${ZARTRUX_ENABLE_IA}")