// src/arch/Cpu.cpp

#include "Cpu.h"
#include <vector>
#include <string>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace zartrux {

class CpuInfo : public ICpuInfo {
public:
    CpuInfo() {
        std::vector<int> cpuInfo(4);
#if defined(_MSC_VER)
        __cpuid(cpuInfo.data(), 1);
#else
        __get_cpuid(1, (unsigned int*)&cpuInfo[0], (unsigned int*)&cpuInfo[1], (unsigned int*)&cpuInfo[2], (unsigned int*)&cpuInfo[3]);
#endif
        m_hasAVX = (cpuInfo[2] & (1 << 28)) != 0;

#if defined(_MSC_VER)
        __cpuidex(cpuInfo.data(), 7, 0);
#else
         __get_cpuid_count(7, 0, (unsigned int*)&cpuInfo[0], (unsigned int*)&cpuInfo[1], (unsigned int*)&cpuInfo[2], (unsigned int*)&cpuInfo[3]);
#endif
        m_hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
        m_hasBMI2 = (cpuInfo[1] & (1 << 8)) != 0;

#if defined(_MSC_VER)
        __cpuidex(cpuInfo.data(), 0x80000001, 0);
#else
        __get_cpuid(0x80000001, (unsigned int*)&cpuInfo[0], (unsigned int*)&cpuInfo[1], (unsigned int*)&cpuInfo[2], (unsigned int*)&cpuInfo[3]);
#endif
        m_hasXOP = (cpuInfo[2] & (1 << 11)) != 0;

        // Detección simple para el bug de JCC en Skylake y derivados
        m_jccErratum = false;
        char vendor[13];
#if defined(_MSC_VER)
        __cpuid(cpuInfo.data(), 0);
#else
         __get_cpuid(0, (unsigned int*)&cpuInfo[0], (unsigned int*)&cpuInfo[1], (unsigned int*)&cpuInfo[2], (unsigned int*)&cpuInfo[3]);
#endif
        memcpy(vendor, &cpuInfo[1], 4);
        memcpy(vendor + 4, &cpuInfo[3], 4);
        memcpy(vendor + 8, &cpuInfo[2], 4);
        vendor[12] = '\0';

        if (strcmp(vendor, "GenuineIntel") == 0) {
#if defined(_MSC_VER)
            __cpuid(cpuInfo.data(), 1);
#else
            __get_cpuid(1, (unsigned int*)&cpuInfo[0], (unsigned int*)&cpuInfo[1], (unsigned int*)&cpuInfo[2], (unsigned int*)&cpuInfo[3]);
#endif
            int family = ((cpuInfo[0] >> 8) & 0xF);
            int model = ((cpuInfo[0] >> 4) & 0xF);
            if (family == 6 && (model == 0x4E || model == 0x5E || model == 0x55)) {
                m_jccErratum = true;
            }
        }
    }

    bool hasBMI2() const override { return m_hasBMI2; }
    bool hasAVX() const override { return m_hasAVX; }
    bool hasAVX2() const override { return m_hasAVX2; }
    bool hasXOP() const override { return m_hasXOP; }
    bool jccErratum() const override { return m_jccErratum; }

private:
    bool m_hasAVX = false;
    bool m_hasAVX2 = false;
    bool m_hasBMI2 = false;
    bool m_hasXOP = false;
    bool m_jccErratum = false;
};

const ICpuInfo* Cpu::info() {
    static const CpuInfo instance;
    return &instance;
}

} // namespace zartrux