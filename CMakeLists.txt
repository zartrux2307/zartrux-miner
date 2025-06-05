cmake_minimum_required(VERSION 3.20)


if(MSVC)
     add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options(/utf-8)
    add_compile_options(/source-charset:utf-8)
    add_compile_options(/execution-charset:utf-8)
    add_definitions(-D_UNICODE -DUNICODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /EHsc /W3")
endif()

project(ZartruxMiner LANGUAGES C CXX)
include(GNUInstallDirs)
option(ZARTRUX_ENABLE_IA "Enable IA modules" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)
endif()

# VCPKG y Toolchain
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "Directorio de vcpkg")
else()
    set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "Directorio de vcpkg")
endif()

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Triplet de vcpkg")
set(CMAKE_WINDOWS_KITS_10_DIR "C:/Program Files (x86)/Windows Kits/10")
list(APPEND CMAKE_PREFIX_PATH
    "C:/Qt/6.9.0/msvc2022_64"
    "C:/Python313"
    "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}"
)

# Dependencias críticas
find_package(Qt6 REQUIRED COMPONENTS Core Quick Network Qml QmlModels Gui)
find_package(OpenSSL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)
if(NOT prometheus-cpp_FOUND)
    message(WARNING "prometheus-cpp no encontrado, intentando compilar desde fuente")
    add_subdirectory(deps/prometheus-cpp EXCLUDE_FROM_ALL)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/randomxzar/src
    ${prometheus-cpp_INCLUDE_DIRS}
    C:/vcpkg/installed/x64-windows/include
     ${VCPKG_INSTALL_PATH}/include
     ${RANDOMX_INCLUDE_DIRS}
   "${CMAKE_WINDOWS_KITS_10_DIR}/Include/10.0.18362.0/um"
    "${CMAKE_WINDOWS_KITS_10_DIR}/Include/10.0.18362.0/shared"
)

# Subdirectorios
add_subdirectory(src/arch)
add_subdirectory(src/core)
add_subdirectory(src/utils)
add_subdirectory(src/runtime)
add_subdirectory(src/network)
add_subdirectory(src/metrics)
add_subdirectory(src/memory)
add_subdirectory(src/security)
add_subdirectory(src/randomxzar)

if(ZARTRUX_ENABLE_IA)
    add_subdirectory(src/ia-modules)
endif()

# Ejecutable principal
add_executable(zartrux-miner src/zarmain.cpp)

target_link_libraries(zartrux-miner PRIVATE
    zartrux::arch
    zartrux::core
    zartrux::utils
    zartrux::runtime
    zartrux::network
    zartrux::memory
    zartrux::metrics
    zartrux::security
    Qt6::Core Qt6::Quick Qt6::Network Qt6::Qml Qt6::QmlModels Qt6::Gui
    OpenSSL::SSL OpenSSL::Crypto
    prometheus-cpp::core
    prometheus-cpp::util
    prometheus-cpp::pull 
    nlohmann_json::nlohmann_json
       fmt::fmt
    cpr::cpr
    randomx
    libzmq
    yaml-cpp::yaml-cpp
)
if(WIN32)
    target_link_libraries(zartrux-miner PRIVATE Pdh.lib)
endif()

# Instalación ejecutable y headers randomx
install(TARGETS zartrux-miner
    EXPORT zartrux_targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}

)


install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/randomxzar/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/randomx
    FILES_MATCHING PATTERN "*.h"
  
)

# Instalar backend web con permisos correctos
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/zarbackend/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/zartrux-miner/zarbackend
    USE_SOURCE_PERMISSIONS
    PATTERN "*.py" PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.log" EXCLUDE
)

# Exportar todos los targets (super-export)
install(EXPORT zartrux_targets
    FILE zartrux_targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zartrux
)

# Configuración post-instalación
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/zartrux-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config.cmake
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zartrux
)

message(STATUS "✅ Configuración completa de ZartruxMiner - Soporte UTF-8 habilitado")
