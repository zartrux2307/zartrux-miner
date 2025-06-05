cmake_minimum_required(VERSION 3.20)
project(zartrux_security VERSION 1.0 LANGUAGES C CXX)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SECURITY_SOURCES
    AESNonceEncryptor.cpp
    ConfigSigner.cpp
)
set(SECURITY_HEADERS
    AESNonceEncryptor.h
    ConfigSigner.h
)

add_library(zartrux_security STATIC ${SECURITY_SOURCES} ${SECURITY_HEADERS})
add_library(zartrux::security ALIAS zartrux_security)

target_include_directories(zartrux_security PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/security>
)
target_link_libraries(zartrux_security PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
)

install(TARGETS zartrux_security
    EXPORT security-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(EXPORT security-targets
    FILE security-targets.cmake
    NAMESPACE zartrux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/security
)

message(STATUS "✅ Módulo 'security' listo con OpenSSL.")
