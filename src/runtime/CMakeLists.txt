cmake_minimum_required(VERSION 3.15)
project(zartrux_runtime VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)

set(runtime_SOURCES
    Profiler.cpp
    AdaptiveScheduler.cpp
    PowerSafe.cpp
    SystemMonitor.cpp
)
set(runtime_HEADERS
    Profiler.h
    AdaptiveScheduler.h
    PowerSafe.h
    SystemMonitor.h
)

add_library(zartrux_runtime STATIC ${runtime_SOURCES} ${runtime_HEADERS})
add_library(zartrux::runtime ALIAS zartrux_runtime)

target_include_directories(zartrux_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/runtime>
)

if(WIN32)
    target_link_libraries(zartrux_runtime PRIVATE Pdh.lib)
    # target_include_directories(zartrux_runtime PRIVATE
    #    "C:/Program Files (x86)/Windows Kits/10/Include/${WINDOWS_SDK_VER}/um"
    # )
endif()

install(TARGETS zartrux_runtime
    EXPORT runtime-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${runtime_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/runtime
)
install(EXPORT runtime-targets
    FILE runtime-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/runtime
)

message(STATUS "Módulo 'runtime' configurado.")
