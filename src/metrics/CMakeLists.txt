cmake_minimum_required(VERSION 3.15)
project(metrics VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)

find_package(prometheus-cpp REQUIRED COMPONENTS core)
find_package(yaml-cpp REQUIRED)

set(METRICS_SOURCES
    PrometheusExporter.cpp
)
set(METRICS_HEADERS
    PrometheusExporter.h
)

add_library(metrics STATIC ${METRICS_SOURCES} ${METRICS_HEADERS})
add_library(zartrux::metrics ALIAS metrics)

target_include_directories(metrics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/metrics>
)
target_link_libraries(metrics PRIVATE
    prometheus-cpp::core
    yaml-cpp::yaml-cpp
    zartrux::core
)


install(TARGETS metrics
    EXPORT zartrux_targets  # <-- Esto debe permanecer
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


message(STATUS "✅ Módulo 'metrics' configurado con Prometheus y yaml-cpp.")
