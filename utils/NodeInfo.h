// src/utils/NodeInfo.h
#pragma once

#include <string>

// ---
// ZARTRUX-MINER v1.0
// Este archivo ofrece utilidades de identificación de nodo, fundamental para logging, métricas y trazabilidad en minería real de Monero (XMR).
// Compatible 100% con ambientes de producción Linux y Windows.
// ---

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
    #include <array>
    #include <limits.h>
#endif

namespace zartrux {

/// Devuelve un identificador único para el nodo, normalmente el hostname del sistema.
/// Usado en backend, Prometheus, monitorización web y exportación de estado JSON.
inline std::string getNodeId() {
#if defined(_WIN32)
    char name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameA(name, &size)) {
        return std::string(name);
    }
#else
    std::array<char, HOST_NAME_MAX + 1> hostname{};
    if (gethostname(hostname.data(), hostname.size()) == 0) {
        return std::string(hostname.data());
    }
#endif
    return "unknown_node";
}

}  // namespace zartrux
