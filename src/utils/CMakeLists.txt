cmake_minimum_required(VERSION 3.15)

project(zartrux_utils VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fuentes y cabeceras ---
set(UTILS_SOURCES
    Logger.cpp
    nonce_logger_adapter.cpp
    config_manager.cpp
    StatusExporter.cpp
)
set(UTILS_HEADERS
    Logger.h
    nonce_logger_adapter.h
    config_manager.h
    NodeInfo.h
    StatusExporter.h 
)

# --- Librería estática ---
add_library(zartrux_utils STATIC ${UTILS_SOURCES})
add_library(zartrux::utils ALIAS zartrux_utils)

target_include_directories(zartrux_utils PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/utils>
)

# --- Dependencias Python y pybind11 si es necesario ---
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

target_link_libraries(zartrux_utils
    PRIVATE
        Python3::Python
        pybind11::module
)

# --- Instalación ---
install(TARGETS zartrux_utils
    EXPORT utils-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${UTILS_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/utils
)

install(EXPORT utils-targets
    FILE zartrux_utils-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/utils
)

message(STATUS "✅ Módulo 'utils' configurado correctamente.")
