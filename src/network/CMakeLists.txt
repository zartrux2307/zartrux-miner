cmake_minimum_required(VERSION 3.15)

project(zartrux_network VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NETWORK_SOURCES
    PoolFailover.cpp
    StratumClient.cpp
)

set(NETWORK_HEADERS
    PoolFailover.h
    StratumClient.h
)

add_library(zartrux_network STATIC ${NETWORK_SOURCES})
add_library(zartrux::network ALIAS zartrux_network)

target_include_directories(zartrux_network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/network>
)

find_package(Qt6 COMPONENTS Network REQUIRED)
target_link_libraries(zartrux_network PUBLIC Qt6::Network)

# Opciones de advertencia multiplataforma
if(MSVC)
    target_compile_options(zartrux_network PRIVATE /W4 /WX)
else()
    target_compile_options(zartrux_network PRIVATE -Wall -Wextra -Werror)
endif()

set_target_properties(zartrux_network PROPERTIES POSITION_INDEPENDENT_CODE ON)

install(TARGETS zartrux_network
    EXPORT network-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${NETWORK_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/network
)

install(EXPORT network-targets
    FILE zartrux_network-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network
)

message(STATUS "✅ Módulo 'network' configurado correctamente.")
