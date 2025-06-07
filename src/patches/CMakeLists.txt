cmake_minimum_required(VERSION 3.15)
project(zartrux_patches LANGUAGES ASM C)

# ------------------------------------------------------------
#   Zartrux Patches Module
# ------------------------------------------------------------
# This module assembles and exports various code fragments and
# the randomx_patch for integration into the RandomX backend.

# Detect platform and assembler
if(MSVC)
    # Windows: use MASM (ml64.exe)
    enable_language(ASM_MASM)
    set(PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/randomx_patch_masm.asm")
    set(ASM_HELPER ? "#MSVC MASM assembler for randomx_patch")
if(MSVC)
    # Ruta al ensamblador ml64.exe (ajustar según instalación)
    find_program(ML64_EXECUTABLE ml64.exe
        HINTS "$ENV{VSINSTALLDIR}/VC/Tools/MSVC" 
        PATH_SUFFIXES "Hostx64/x64"
    )
    if(NOT ML64_EXECUTABLE)
        message(FATAL_ERROR "❌ No se encontró ml64.exe para ensamblar con MASM.")
    endif()
elseif(CMAKE_ASM_NASM_COMPILER)
    # Linux/macOS: use NASM
    enable_language(ASM_NASM)
    set(PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/randomx_patch_nasm.asm")
    set(ASM_HELPER ? "#NASM assembler for randomx_patch")
else()
    message(WARNING "No supported assembler found; patches module will be empty.")
    return()
endif()

# Gather include fragments (.inc) to be packaged as resources
file(GLOB INC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.inc"
)

# Add the patch assembly object
if(EXISTS ${PATCH_FILE})
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/randomx_patch.obj
        COMMAND ${CMAKE_ASM_MASM_COMPILER} $<$<BOOL:${MSVC}>:/c> $<$<NOT:$<BOOL:${MSVC}>>:/f> \
            /Fo${CMAKE_CURRENT_BINARY_DIR}/randomx_patch.obj \
            "${PATCH_FILE}"
        DEPENDS "${PATCH_FILE}"
        COMMENT "Assembling patch: ${PATCH_FILE}"
        VERBATIM
    )
    add_library(randomx_patch_obj OBJECT ${CMAKE_CURRENT_BINARY_DIR}/randomx_patch.obj)
    set_target_properties(randomx_patch_obj PROPERTIES
        POSITION_INDEPENDENT_CODE OFF
    )
else()
    message(FATAL_ERROR "Patch file not found: ${PATCH_FILE}")
endif()

# Optional: export inc files into a C header for embedding, or install
install(FILES ${INC_FILES}
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/patches/inc
)

# Export the patch object for superprojects
export(TARGETS randomx_patch_obj NAMESPACE zartrux:: FILE zartrux_patchesTargets.cmake)

message(STATUS "Zartrux patches module configured with ${ASM_HELPER}")
