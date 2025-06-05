cmake_minimum_required(VERSION 3.15)

project(ZartruxPatches VERSION 1.0 LANGUAGES ASM)

# Mensaje inicial amigable
message(STATUS "⚙️ Activando parches personalizados para kernels RandomX...")

# Busca la ruta de randomx original (ajusta si la tienes en otra ubicación)
set(RANDOMX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../randomxzar/src/randomx")
if(EXISTS ${RANDOMX_PATH}/CMakeLists.txt)
    add_subdirectory(${RANDOMX_PATH} ${CMAKE_BINARY_DIR}/build_randomxzar)
    message(STATUS "✅ randomx_core agregado correctamente desde: ${RANDOMX_PATH}")
else()
    message(FATAL_ERROR "❌ No se encontró randomx original en: ${RANDOMX_PATH}")
endif()

# Buscar y agregar el parche ASM si existe
set(PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/randomx_patch.asm")
if(EXISTS ${PATCH_FILE})
    message(STATUS "✅ Archivo randomx_patch.asm localizado: ${PATCH_FILE}")

    # Crea la librería estática sólo si el archivo existe
    add_library(randomx_patch STATIC ${PATCH_FILE})

    set_target_properties(randomx_patch PROPERTIES
        LANGUAGE ASM
        POSITION_INDEPENDENT_CODE ON
    )

    # Vincula con randomx_core si existe
    target_link_libraries(randomx_patch PRIVATE randomx_core)

    # Exportar include_dir para integración fácil
    target_include_directories(randomx_patch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
else()
    message(WARNING "⚠️ Archivo randomx_patch.asm no encontrado. Si no se necesita, puedes ignorar este aviso.")
endif()

# Permite incluir este módulo en superproyectos (ej: Zartrux)
install(TARGETS randomx_patch
    EXPORT randomx_patch-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(EXPORT randomx_patch-targets
    FILE randomx_patch-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/patches
)

message(STATUS "✅ Parcheo y configuración de kernels RandomX completados")
