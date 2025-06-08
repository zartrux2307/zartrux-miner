cmake_minimum_required(VERSION 3.20)

# Project definition with enhanced metadata
project(ZartruxMiner
    VERSION 1.0.0
    DESCRIPTION "Professional Cryptocurrency Mining Solution"
    HOMEPAGE_URL "https://github.com/zartrux2307/zartrux-miner"
    LANGUAGES C CXX ASM  # Añadido ASM para soporte de ensamblador
)

# Include standard CMake modules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)

# Project options
option(ZARTRUX_ENABLE_IA "Enable IA modules" ON)
option(ZARTRUX_BUILD_TESTS "Build test suite" OFF)
option(ZARTRUX_ENABLE_LTO "Enable Link Time Optimization" ON)

# C++ configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific configurations
if(MSVC)
    add_compile_options(
        /utf-8     # UTF-8 source encoding
        /W4        # Warning level 4
        /WX        # Treat warnings as errors
        /permissive-  # Strict C++ conformance
        /Zc:preprocessor # Modern preprocessor
        /EHsc      # Standard C++ exception handling
    )
    add_definitions(
        -D_UNICODE 
        -DUNICODE
        -D_WIN32_WINNT=0x0A00  # Target Windows 10
        -DNOMINMAX 
        -D_USE_MATH_DEFINES
        -DWIN32_LEAN_AND_MEAN
        -D_CRT_SECURE_NO_WARNINGS
    )
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -march=native
        -fPIC
    )
endif()

# VCPKG configuration with proper error handling
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "vcpkg root directory")
elseif(EXISTS "C:/vcpkg")
    set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "vcpkg root directory")
else()
    message(FATAL_ERROR "VCPKG_ROOT not found. Please set VCPKG_ROOT environment variable")
endif()

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" 
    CACHE FILEPATH "vcpkg toolchain file")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")

# Qt6 configuration with version check
set(QT_MIN_VERSION "6.9.0")
set(Qt6_DIR "C:/Qt/6.9.0/msvc2022_64/lib/cmake/Qt6" CACHE PATH "Qt6 CMake config directory")
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.9.0/msvc2022_64")

# Dependencies with version requirements
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS 
    Core Quick Network Qml QmlModels Gui)
find_package(OpenSSL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/patches  # Para archivos .inc
)

# Component subdirectories
set(ZARTRUX_COMPONENTS
    arch
    core
    utils
    runtime
    network
    memory
    security
    crypto  # Cambiado de randomxzar a crypto
)

foreach(component ${ZARTRUX_COMPONENTS})
    add_subdirectory(src/${component})
endforeach()

if(ZARTRUX_ENABLE_IA)
    add_subdirectory(src/ia-modules)
endif()

# Main executable
add_executable(zartrux-miner src/zarmain.cpp)

# Target-specific configurations
target_include_directories(zartrux-miner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/randomx
    ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include
)

# Enhanced library linking with clear grouping
target_link_libraries(zartrux-miner PRIVATE
    # Internal components
    zartrux::arch
    zartrux::core
    zartrux::utils
    zartrux::runtime
    zartrux::network
    zartrux::memory
    zartrux::security
    randomxzar  # Nombre corregido

    # Qt components
    Qt6::Core 
    Qt6::Quick 
    Qt6::Network 
    Qt6::Qml 
    Qt6::QmlModels 
    Qt6::Gui

    # External dependencies
    OpenSSL::SSL 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    fmt::fmt
    cpr::cpr
    yaml-cpp::yaml-cpp
    ${ZMQ_LIBRARIES}  # Usar variable correcta
    Threads::Threads

    # Platform-specific libraries
    $<$<PLATFORM_ID:Windows>:Pdh.lib>
)

# Installation configuration
install(TARGETS zartrux-miner randomxzar
    EXPORT zartrux_targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zartrux
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers - Ruta corregida
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/randomx/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zartrux/crypto/randomx
    FILES_MATCHING 
    PATTERN "*.h" 
    PATTERN "*.hpp"
)

# Install backend scripts with proper permissions
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

# Install configuration files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/config/
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/zartrux-miner
    FILES_MATCHING PATTERN "*.json"
)

# Export targets
install(EXPORT zartrux_targets
    FILE zartrux-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zartrux
)

# Generate and install package configuration
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/zartrux-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zartrux
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zartrux-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zartrux
)

# Enable solution folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Enable Link Time Optimization for Release builds
if(ZARTRUX_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "✅ LTO enabled for Release builds")
    else()
        message(WARNING "❌ LTO is not supported: ${error}")
        set(ipo_supported OFF)
    endif()
else()
    set(ipo_supported OFF)
endif()

# Status messages
message(STATUS "✨ ZartruxMiner Configuration Summary:")
message(STATUS "  📦 Version: ${PROJECT_VERSION}")
message(STATUS "  🔨 Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  📋 C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  🔄 IA Modules: ${ZARTRUX_ENABLE_IA}")
message(STATUS "  🚀 LTO Support: ${ipo_supported}")
message(STATUS "  🎯 Target Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  📍 Install Prefix: ${CMAKE_INSTALL_PREFIX}")