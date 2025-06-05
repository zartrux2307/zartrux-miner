cmake_minimum_required(VERSION 3.15)
project(zartrux_arch VERSION 1.0.0 LANGUAGES CXX ASM_NASM)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Archivos fuente del kernel optimizado para CPU x86
set(ARCH_SOURCES
    x8ag/kernel_x86_generic.cpp
    x8ag/kernel_x86_sse2.cpp
    x8ag/kernel_x86_avx.cpp
    x8ag/kernel_x86_avx512.cpp
)

# Agrega la librería
add_library(zartrux_arch STATIC ${ARCH_SOURCES})
add_library(zartrux::arch ALIAS zartrux_arch)

target_include_directories(zartrux_arch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/x8ag>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/arch>
)


install(TARGETS zartrux_arch
    EXPORT arch-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT arch-targets
    FILE arch-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arch
)

message(STATUS "✔ Módulo 'arch' preparado con AVX2/AVX512.")
