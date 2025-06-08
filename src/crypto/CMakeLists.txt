cmake_minimum_required(VERSION 3.15)
project(ZartruxCrypto LANGUAGES C CXX ASM)

enable_language(ASM)

# Fuentes principales
file(GLOB_RECURSE CRYPTO_SOURCES
    "rx/*.cpp"
    "rx/*.h"
    "randomx/*.cpp"
    "randomx/*.h"
    "randomx/*.hpp"
    "randomx/*.c"
)

# Archivos Blake2
set(BLAKE2_SOURCES
    "randomx/blake2/blake2b.c"
    "randomx/blake2/blake2b_sse41.c"
    "randomx/blake2/avx2/blake2b_avx2.c"
)

# Archivos .inc necesarios para compilación
set(PATCHES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../patches")
set(INCLUDE_FILES
    "${PATCHES_DIR}/program_epilogue_win64.inc"
    "${PATCHES_DIR}/program_prologue_win64.inc"
    "${PATCHES_DIR}/program_read_dataset.inc"
    "${PATCHES_DIR}/randomx_reciprocal.inc"
)

# Archivos de ensamblador
if(MSVC)
    set(ASM_SOURCES "${PATCHES_DIR}/randomx_patch_masm.asm")
    set_source_files_properties(${ASM_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
else()
    set(ASM_SOURCES "${PATCHES_DIR}/randomx_patch_nasm.asm")
    set_source_files_properties(${ASM_SOURCES} PROPERTIES LANGUAGE ASM_NASM)
endif()

# Crear biblioteca estática
add_library(randomxzar STATIC
    ${CRYPTO_SOURCES}
    ${ASM_SOURCES}
    ${BLAKE2_SOURCES}
)

# Configuración de arquitectura
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|x64|AMD64")
    target_compile_definitions(randomxzar PRIVATE 
        RANDOMX_X86=1
        RANDOMX_HAVE_COMPILER=1
    )
    if(MSVC)
        target_compile_options(randomxzar PRIVATE /arch:AVX2)
    else()
        target_compile_options(randomxzar PRIVATE -mavx2 -mbmi2 -maes)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
    target_compile_definitions(randomxzar PRIVATE RANDOMX_ARM=1)
    if(NOT MSVC)
        target_compile_options(randomxzar PRIVATE -march=armv8-a+crypto)
    endif()
endif()

# Inclusión de headers (✅ corregido con BUILD_INTERFACE)
target_include_directories(randomxzar PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/randomx>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/randomx/blake2>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/randomx/blake2/avx2>
    $<BUILD_INTERFACE:${PATCHES_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Copiar archivos .inc durante la compilación
add_custom_command(TARGET randomxzar PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PATCHES_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/patches"
    COMMENT "Copiando archivos .inc para compilación"
    VERBATIM
)

# Mensaje de estado
message(STATUS "✅ Módulo crypto/randomx configurado:")
message(STATUS "  - Fuentes: ${CRYPTO_SOURCES}")
message(STATUS "  - Ensamblador: ${ASM_SOURCES}")
message(STATUS "  - Archivos .inc: ${INCLUDE_FILES}")
message(STATUS "  - Arquitectura objetivo: ${CMAKE_SYSTEM_PROCESSOR}")

# Instalación de headers
install(DIRECTORY 
    ${CMAKE_CURRENT_SOURCE_DIR}/randomx/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zartrux/crypto/randomx
    FILES_MATCHING PATTERN "*.h*"
)

# Instalación de archivos .inc
install(DIRECTORY ${PATCHES_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zartrux/crypto/randomx/patches
    FILES_MATCHING PATTERN "*.inc"
)
