// src/arch/Cpu.h

#pragma once

#include <cstdint>

namespace zartrux {

class ICpuInfo {
public:
    virtual ~ICpuInfo() = default;
    virtual bool hasBMI2() const = 0;
    virtual bool hasAVX() const = 0;
    virtual bool hasAVX2() const = 0;
    virtual bool hasXOP() const = 0;
    virtual bool jccErratum() const = 0;
};

class Cpu {
public:
    static const ICpuInfo* info();
};

} // namespace zartrux