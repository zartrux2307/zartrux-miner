#include <stdexcept>
#include <cstring>
#include <climits>
#include <atomic>
#include "crypto/randomx/jit_compiler_x86.hpp"
#include "crypto/common/VirtualMemory.h"
#include "crypto/randomx/jit_compiler_x86_static.hpp"
#include "crypto/randomx/program.hpp"
#include "crypto/randomx/reciprocal.h"
#include "crypto/randomx/superscalar.hpp"
#include "crypto/randomx/virtual_memory.hpp"
#include "runtime/Profiler.h"

// Cabeceras para la detección de CPU
#if defined(_MSC_VER)
#   include <intrin.h>
#else
#   include <cpuid.h>
#endif

static bool hugePagesJIT = false;
static int optimizedDatasetInit = -1;

void randomx_set_huge_pages_jit(bool hugePages){
    hugePagesJIT = hugePages;
}

void randomx_set_optimized_dataset_init(int value){
    optimizedDatasetInit = value;
}

namespace randomx {
    /*
    * En esta sección se detalla la asignación de registros de la CPU para
    * la máquina virtual de RandomX. Cada registro tiene un propósito específico,
    * desde almacenamiento temporal hasta punteros a memoria o contadores.
    * Es fundamental para entender el código ensamblador que se genera dinámicamente.
    */
	// ... (comentarios de REGISTER ALLOCATION) ...
#   if defined(_MSC_VER) && (defined(_DEBUG) || defined (RELWITHDEBINFO))
    #define ADDR(x) ((((uint8_t*)&x)[0] == 0xE9) ? (((uint8_t*)&x) + *(const int32_t*)(((uint8_t*)&x) + 1) + 5) : ((uint8_t*)&x))
#   else
    #define ADDR(x) ((uint8_t*)&x)
#   endif

    #define codePrologue ADDR(randomx_program_prologue)
    #define codeLoopBegin ADDR(randomx_program_loop_begin)
    #define codeLoopLoad ADDR(randomx_program_loop_load)
    #define codeLoopLoadXOP ADDR(randomx_program_loop_load_xop)
    #define codeProgramStart ADDR(randomx_program_start)
    #define codeReadDataset ADDR(randomx_program_read_dataset)
    #define codeReadDatasetLightSshInit ADDR(randomx_program_read_dataset_sshash_init)
    #define codeReadDatasetLightSshFin ADDR(randomx_program_read_dataset_sshash_fin)
    #define codeDatasetInit ADDR(randomx_dataset_init)
    #define codeDatasetInitAVX2Prologue ADDR(randomx_dataset_init_avx2_prologue)
    #define codeDatasetInitAVX2LoopEnd ADDR(randomx_dataset_init_avx2_loop_end)
    #define codeDatasetInitAVX2Epilogue ADDR(randomx_dataset_init_avx2_epilogue)
    #define codeDatasetInitAVX2SshLoad ADDR(randomx_dataset_init_avx2_ssh_load)
    #define codeDatasetInitAVX2SshPrefetch ADDR(randomx_dataset_init_avx2_ssh_prefetch)
    #define codeLoopStore ADDR(randomx_program_loop_store)
    #define codeLoopEnd ADDR(randomx_program_loop_end)
    #define codeEpilogue ADDR(randomx_program_epilogue)
    #define codeProgramEnd ADDR(randomx_program_end)
    #define codeSshLoad ADDR(randomx_sshash_load)
    #define codeSshPrefetch ADDR(randomx_sshash_prefetch)
    #define codeSshEnd ADDR(randomx_sshash_end)
    #define codeSshInit ADDR(randomx_sshash_init)

    #define prologueSize (codeLoopBegin - codePrologue)
    #define loopLoadSize (codeLoopLoadXOP - codeLoopLoad)
    #define loopLoadXOPSize (codeProgramStart - codeLoopLoadXOP)
    #define readDatasetSize (codeReadDatasetLightSshInit - codeReadDataset)
    #define readDatasetLightInitSize (codeReadDatasetLightSshFin - codeReadDatasetLightSshInit)
    #define readDatasetLightFinSize (codeLoopStore - codeReadDatasetLightSshFin)
    #define loopStoreSize (codeLoopEnd - codeLoopStore)
    #define datasetInitSize (codeDatasetInitAVX2Prologue - codeDatasetInit)
    #define datasetInitAVX2PrologueSize (codeDatasetInitAVX2LoopEnd - codeDatasetInitAVX2Prologue)
    #define datasetInitAVX2LoopEndSize (codeDatasetInitAVX2Epilogue - codeDatasetInitAVX2LoopEnd)
    #define datasetInitAVX2EpilogueSize (codeDatasetInitAVX2SshLoad - codeDatasetInitAVX2Epilogue)
    #define datasetInitAVX2SshLoadSize (codeDatasetInitAVX2SshPrefetch - codeDatasetInitAVX2SshLoad)
    #define datasetInitAVX2SshPrefetchSize (codeEpilogue - codeDatasetInitAVX2SshPrefetch)
    #define epilogueSize (codeSshLoad - codeEpilogue)
    #define codeSshLoadSize (codeSshPrefetch - codeSshLoad)
    #define codeSshPrefetchSize (codeSshEnd - codeSshPrefetch)
    #define codeSshInitSize (codeProgramEnd - codeSshInit)

    #define epilogueOffset ((CodeSize - epilogueSize) & ~63)

    constexpr int32_t superScalarHashOffset = 32768;

    static const uint8_t NOP1[] = { 0x90 };
    static const uint8_t NOP2[] = { 0x66, 0x90 };
    static const uint8_t NOP3[] = { 0x66, 0x66, 0x90 };
    static const uint8_t NOP4[] = { 0x0F, 0x1F, 0x40, 0x00 };
    static const uint8_t NOP5[] = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    static const uint8_t NOP6[] = { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    static const uint8_t NOP7[] = { 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t NOP8[] = { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t NOP9[] = { 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };

    static const uint8_t* NOPX[] = { NOP1, NOP2, NOP3, NOP4, NOP5, NOP6, NOP7, NOP8, NOP9 };

    static const uint8_t NOP13[] = { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    static const uint8_t NOP14[] = { 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t NOP25[] = { 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t NOP26[] = { 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };

    static const uint8_t JMP_ALIGN_PREFIX[14][16] = {
        {}, {0x2E}, {0x2E, 0x2E}, {0x2E, 0x2E, 0x2E}, {0x2E, 0x2E, 0x2E, 0x2E},
        {0x2E, 0x2E, 0x2E, 0x2E, 0x2E}, {0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E},
        {0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E}, {0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E},
        {0x90, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E},
        {0x66, 0x90, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E},
        {0x66, 0x66, 0x90, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E},
        {0x0F, 0x1F, 0x40, 0x00, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E},
        {0x0F, 0x1F, 0x44, 0x00, 0x00, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E},
    };

    static inline uint8_t* alignToPage(uint8_t* p, size_t pageSize) {
        size_t k = (size_t)p;
        k -= k % pageSize;
        return (uint8_t*)k;
    }

    size_t JitCompilerX86::getCodeSize() {
        return codePos < prologueSize ? 0 : codePos - prologueSize;
    }

    void JitCompilerX86::enableWriting() const {
        uint8_t* p1 = alignToPage(code, 4096);
        uint8_t* p2 = code + CodeSize;
        zartrux::VirtualMemory::protectRW(p1, p2 - p1);
    }

    void JitCompilerX86::enableExecution() const {
        uint8_t* p1 = alignToPage(code, 4096);
        uint8_t* p2 = code + CodeSize;
        zartrux::VirtualMemory::protectRX(p1, p2 - p1);
    }
    
#   ifdef _MSC_VER
    static FORCE_INLINE uint32_t rotl32(uint32_t a, int shift) { return _rotl(a, shift); }
#   else
    static FORCE_INLINE uint32_t rotl32(uint32_t a, int shift) { return (a << shift) | (a >> (-shift & 31)); }
#   endif

    static std::atomic<size_t> codeOffset;
    constexpr size_t codeOffsetIncrement = 59 * 64;

    JitCompilerX86::JitCompilerX86(bool hugePagesEnable, bool optimizedInitDatasetEnable) {
        
        // --- INICIO DEL CÓDIGO ADAPTADO PARA ZARTRUX-MINER ---

        // Función de ayuda para llamar a CPUID de forma portable
        auto cpuid = [](int leaf, int subleaf, int* info) {
        #if defined(_MSC_VER)
            __cpuidex(info, leaf, subleaf);
        #else
            __get_cpuid_count(leaf, subleaf, (unsigned int*)&info[0], (unsigned int*)&info[1], (unsigned int*)&info[2], (unsigned int*)&info[3]);
        #endif
        };

        int info[4];

        // 1. Detección de sets de instrucciones (AVX, AVX2, XOP)
        cpuid(1, 0, info);
        this->hasAVX = (info[2] & (1 << 28)) != 0;
        
        cpuid(7, 0, info);
        this->hasAVX2 = (info[1] & (1 << 5)) != 0;
        
        cpuid(static_cast<int>(0x80000001), 0, info);
        this->hasXOP = (info[2] & (1 << 11)) != 0;

        // 2. Detección del fabricante
        enum Vendor { VENDOR_UNKNOWN, VENDOR_INTEL, VENDOR_AMD };
        Vendor cpu_vendor = VENDOR_UNKNOWN;
        char vendor_str[13];
        cpuid(0, 0, info);
        memcpy(vendor_str, &info[1], 4);
        memcpy(vendor_str + 4, &info[3], 4);
        memcpy(vendor_str + 8, &info[2], 4);
        vendor_str[12] = '\0';

        if (strcmp(vendor_str, "GenuineIntel") == 0) {
            cpu_vendor = VENDOR_INTEL;
        } else if (strcmp(vendor_str, "AuthenticAMD") == 0) {
            cpu_vendor = VENDOR_AMD;
        }

        // 3. Detección de arquitectura AMD Zen (simplificado)
        enum Arch { ARCH_UNKNOWN, ARCH_LEGACY, ARCH_ZEN, ARCH_ZEN_PLUS, ARCH_ZEN2, ARCH_ZEN3, ARCH_ZEN4, ARCH_ZEN5 };
        Arch cpu_arch = ARCH_UNKNOWN;
        if (cpu_vendor == VENDOR_AMD) {
            cpuid(1, 0, info);
            int family = ((info[0] >> 8) & 0xF) + ((info[0] >> 20) & 0xFF);
            int model = ((info[0] >> 4) & 0xF) | ((info[0] >> 12) & 0xF0);

            if (family == 0x17) { // Familia Zen/Zen+/Zen2
                if (model >= 0x30 && model <= 0x7F) cpu_arch = ARCH_ZEN2;
                else if (model >= 0x01 && model <= 0x1F) cpu_arch = ARCH_ZEN;
                else cpu_arch = ARCH_ZEN_PLUS;
            } else if (family == 0x19) { // Familia Zen3/Zen4
                if (model >= 0x60 && model <= 0x7F) cpu_arch = ARCH_ZEN4;
                else cpu_arch = ARCH_ZEN3;
            } else if (family == 0x1A) { // Familia Zen5 (estimado)
                cpu_arch = ARCH_ZEN5;
            } else {
                cpu_arch = ARCH_LEGACY;
            }
        }
        
        // 4. Detección de núcleos e hilos (simplificado)
        int cpu_cores = 1;
        int cpu_threads = 1;
        cpuid(1, 0, info);
        if ((info[3] & (1 << 28)) != 0) { // Soporte para Hyper-Threading
            cpu_threads = (info[1] >> 16) & 0xFF;
            if (cpu_vendor == VENDOR_INTEL) {
                // Para Intel, se necesita una lógica más compleja para núcleos,
                // pero esto es una aproximación razonable.
                cpuid(0, 0, info);
                if (info[0] >= 0x0B) {
                    cpuid(0x0B, 1, info);
                    cpu_cores = (info[1] & 0xFFFF);
                } else {
                     cpu_cores = cpu_threads / 2; // Estimación simple
                }
            } else if (cpu_vendor == VENDOR_AMD) {
                cpuid(static_cast<int>(0x80000008), 0, info);
                cpu_cores = (info[2] & 0xFF) + 1;
            }
        } else {
            cpu_cores = cpu_threads = 1;
        }
        
        // 5. jccErratum (Comportamiento específico de Intel Skylake)
        this->BranchesWithin32B = false;
        if (cpu_vendor == VENDOR_INTEL) {
            cpuid(1, 0, info);
            int family = (info[0] >> 8) & 0xF;
            int model = ((info[0] >> 4) & 0xF) + ((info[0] >> 12) & 0xF0);
            if (family == 6 && (model == 0x4E || model == 0x5E || model == 0x55 || model == 0x8E || model == 0x9E)) {
                this->BranchesWithin32B = true; // Afecta a Skylake, Kaby Lake, etc.
            }
        }
        
        // --- Lógica original, ahora usando nuestras variables ---

        // Deshabilitado por defecto
        this->initDatasetAVX2 = false;

        if (optimizedInitDatasetEnable) {
            if (optimizedDatasetInit > 0) {
                this->initDatasetAVX2 = true;
            } else if (optimizedDatasetInit < 0) {
                if (cpu_vendor == VENDOR_INTEL) {
                    // AVX2 init es más rápido en Intel sin Hyper-Threading
                    this->initDatasetAVX2 = (cpu_cores == cpu_threads);
                } else if (cpu_vendor == VENDOR_AMD) {
                    switch (cpu_arch) {
                        case ARCH_ZEN:
                        case ARCH_ZEN_PLUS:
                        default:
                            this->initDatasetAVX2 = false; // Más lento en Zen/Zen+
                            break;
                        case ARCH_ZEN2:
                            // Más rápido en Zen2 sin SMT (CPUs móviles)
                            this->initDatasetAVX2 = (cpu_cores == cpu_threads);
                            break;
                        case ARCH_ZEN3:
                        case ARCH_ZEN5:
                            this->initDatasetAVX2 = true; // Más rápido en Zen3 y Zen5
                            break;
                        case ARCH_ZEN4:
                            this->initDatasetAVX2 = false; // Más lento en Zen4
                            break;
                    }
                }
            }
        }

        // Verificación final: si no hay AVX2, no se puede usar esta optimización.
        if (!this->hasAVX2) {
            this->initDatasetAVX2 = false;
        }

        // --- FIN DEL CÓDIGO ADAPTADO ---

        allocatedSize = this->initDatasetAVX2 ? (CodeSize * 4) : (CodeSize * 2);
        allocatedCode = static_cast<uint8_t*>(allocExecutableMemory(allocatedSize,
#           ifdef XMRIG_SECURE_JIT
            false
#           else
            hugePagesJIT && hugePagesEnable
#           endif
        ));

        // Shift code base address to improve caching - all threads will use different L2/L3 cache sets
        code = allocatedCode + (codeOffset.fetch_add(codeOffsetIncrement) % CodeSize);

        memcpy(code, codePrologue, prologueSize);
        if (this->hasXOP) {
            memcpy(code + prologueSize, codeLoopLoadXOP, loopLoadXOPSize);
        }
        else {
            memcpy(code + prologueSize, codeLoopLoad, loopLoadSize);
        }
        memcpy(code + epilogueOffset, codeEpilogue, epilogueSize);

        codePosFirst = prologueSize + (this->hasXOP ? loopLoadXOPSize : loopLoadSize);
#       ifdef XMRIG_FIX_RYZEN
        mainLoopBounds.first = code + prologueSize;
        mainLoopBounds.second = code + epilogueOffset;
#       endif
    }

    JitCompilerX86::~JitCompilerX86() {
        codeOffset.fetch_sub(codeOffsetIncrement);
        freePagedMemory(allocatedCode, allocatedSize);
    }

    template<size_t N>
    static FORCE_INLINE void prefetch_data(const void* data) {
        rx_prefetch_nta(data);
        prefetch_data<N - 1>(reinterpret_cast<const char*>(data) + 64);
    }

    template<> FORCE_INLINE void prefetch_data<0>(const void*) {}

    template<typename T> static FORCE_INLINE void prefetch_data(const T& data) { prefetch_data<(sizeof(T) + 63) / 64>(&data); }

    void JitCompilerX86::prepare() {
        prefetch_data(engine);
        prefetch_data(RandomX_CurrentConfig);
    }

    void JitCompilerX86::generateProgram(Program& prog, ProgramConfiguration& pcfg, uint32_t flags) {
        PROFILE_SCOPE(RandomX_JIT_compile);
#       ifdef XMRIG_SECURE_JIT
        enableWriting();
#       endif

        vm_flags = flags;

        generateProgramPrologue(prog, pcfg);
        emit(codeReadDataset, readDatasetSize, code, codePos);
        generateProgramEpilogue(prog, pcfg);
    }

    void JitCompilerX86::generateProgramLight(Program& prog, ProgramConfiguration& pcfg, uint32_t datasetOffset) {
        generateProgramPrologue(prog, pcfg);
        emit(codeReadDatasetLightSshInit, readDatasetLightInitSize, code, codePos);
        *(uint32_t*)(code + codePos) = 0xc381;
        codePos += 2;
        emit32(datasetOffset / CacheLineSize, code, codePos);
        emitByte(0xe8, code, codePos);
        emit32(superScalarHashOffset - (codePos + 4), code, codePos);
        emit(codeReadDatasetLightSshFin, readDatasetLightFinSize, code, codePos);
        generateProgramEpilogue(prog, pcfg);
    }

    template<size_t N>
    void JitCompilerX86::generateSuperscalarHash(SuperscalarProgram(&programs)[N]) {
        uint8_t* p = code;
        if (initDatasetAVX2) {
            codePos = 0;
            emit(codeDatasetInitAVX2Prologue, datasetInitAVX2PrologueSize, code, codePos);

            for (unsigned j = 0; j < RandomX_CurrentConfig.CacheAccesses; ++j) {
                SuperscalarProgram& prog = programs[j];
                uint32_t pos = codePos;
                for (uint32_t i = 0, n = prog.getSize(); i < n; ++i) {
                    generateSuperscalarCode<true>(prog(i), p, pos);
                }
                codePos = pos;
                emit(codeSshLoad, codeSshLoadSize, code, codePos);
                emit(codeDatasetInitAVX2SshLoad, datasetInitAVX2SshLoadSize, code, codePos);
                if (j < RandomX_CurrentConfig.CacheAccesses - 1) {
                    *(uint32_t*)(code + codePos) = 0xd88b49 + (static_cast<uint32_t>(prog.getAddressRegister()) << 16);
                    codePos += 3;
                    emit(RandomX_CurrentConfig.codeSshPrefetchTweaked, codeSshPrefetchSize, code, codePos);
                    uint8_t* p_temp = code + codePos;
                    emit(codeDatasetInitAVX2SshPrefetch, datasetInitAVX2SshPrefetchSize, code, codePos);
                    p_temp[3] += prog.getAddressRegister() << 3;
                }
            }

            emit(codeDatasetInitAVX2LoopEnd, datasetInitAVX2LoopEndSize, code, codePos);

            // Number of bytes from the start of randomx_dataset_init_avx2_prologue to loop_begin label
            constexpr int32_t prologue_size = 320;
            *(int32_t*)(code + codePos - 4) = prologue_size - codePos;

            emit(codeDatasetInitAVX2Epilogue, datasetInitAVX2EpilogueSize, code, codePos);
            return;
        }

        memcpy(code + superScalarHashOffset, codeSshInit, codeSshInitSize);
        codePos = superScalarHashOffset + codeSshInitSize;
        for (unsigned j = 0; j < RandomX_CurrentConfig.CacheAccesses; ++j) {
            SuperscalarProgram& prog = programs[j];
            uint32_t pos = codePos;
            for (uint32_t i = 0, n = prog.getSize(); i < n; ++i) {
                generateSuperscalarCode<false>(prog(i), p, pos);
            }
            codePos = pos;
            emit(codeSshLoad, codeSshLoadSize, code, codePos);
            if (j < RandomX_CurrentConfig.CacheAccesses - 1) {
                *(uint32_t*)(code + codePos) = 0xd88b49 + (static_cast<uint32_t>(prog.getAddressRegister()) << 16);
                codePos += 3;
                emit(RandomX_CurrentConfig.codeSshPrefetchTweaked, codeSshPrefetchSize, code, codePos);
            }
        }
        emitByte(0xc3, code, codePos);
    }

    template
    void JitCompilerX86::generateSuperscalarHash(SuperscalarProgram(&programs)[RANDOMX_CACHE_MAX_ACCESSES]);

    void JitCompilerX86::generateDatasetInitCode() {
        // AVX2 code is generated in generateSuperscalarHash()
        if (!initDatasetAVX2) {
            memcpy(code, codeDatasetInit, datasetInitSize);
        }
    }

    void JitCompilerX86::generateProgramPrologue(Program& prog, ProgramConfiguration& pcfg) {
        codePos = ADDR(randomx_program_prologue_first_load) - ADDR(randomx_program_prologue);
        *(uint32_t*)(code + codePos + 4) = RandomX_CurrentConfig.ScratchpadL3Mask64_Calculated;
        *(uint32_t*)(code + codePos + 14) = RandomX_CurrentConfig.ScratchpadL3Mask64_Calculated;
        if (hasAVX) {
            uint32_t* p = (uint32_t*)(code + codePos + 61);
            *p = (*p & 0xFF000000U) | 0x0077F8C5U; // vzeroupper
        }
#       ifdef XMRIG_FIX_RYZEN
        xmrig::RxFix::setMainLoopBounds(mainLoopBounds);
#       endif

        imul_rcp_storage = code + (ADDR(randomx_program_imul_rcp_store) - codePrologue) + 2;
        imul_rcp_storage_used = 0;

        memcpy(imul_rcp_storage - 34, &pcfg.eMask, sizeof(pcfg.eMask));
        codePos = codePosFirst;
        prevCFROUND = -1;
        prevFPOperation = -1;

        //mark all registers as used
        uint64_t* r = (uint64_t*)registerUsage;
        uint64_t k = codePos;
        k |= k << 32;
        for (unsigned j = 0; j < RegistersCount / 2; ++j) {
            r[j] = k;
        }

        for (int i = 0, n = static_cast<int>(RandomX_CurrentConfig.ProgramSize); i < n; i += 4) {
            Instruction& instr1 = prog(i);
            Instruction& instr2 = prog(i + 1);
            Instruction& instr3 = prog(i + 2);
            Instruction& instr4 = prog(i + 3);

            InstructionGeneratorX86 gen1 = engine[instr1.opcode];
            InstructionGeneratorX86 gen2 = engine[instr2.opcode];
            InstructionGeneratorX86 gen3 = engine[instr3.opcode];
            InstructionGeneratorX86 gen4 = engine[instr4.opcode];

            (*gen1)(this, instr1);
            (*gen2)(this, instr2);
            (*gen3)(this, instr3);
            (*gen4)(this, instr4);
        }

        *(uint64_t*)(code + codePos) = 0xc03341c08b41ull + (static_cast<uint64_t>(pcfg.readReg2) << 16) + (static_cast<uint64_t>(pcfg.readReg3) << 40);
        codePos += 6;
    }

    void JitCompilerX86::generateProgramEpilogue(Program& prog, ProgramConfiguration& pcfg) {
        *(uint64_t*)(code + codePos) = 0xc03349c08b49ull + (static_cast<uint64_t>(pcfg.readReg0) << 16) + (static_cast<uint64_t>(pcfg.readReg1) << 40);
        codePos += 6;
        emit(RandomX_CurrentConfig.codePrefetchScratchpadTweaked, RandomX_CurrentConfig.codePrefetchScratchpadTweakedSize, code, codePos);
        memcpy(code + codePos, codeLoopStore, loopStoreSize);
        codePos += loopStoreSize;

        if (BranchesWithin32B) {
            const uint32_t branch_begin = static_cast<uint32_t>(codePos);
            const uint32_t branch_end = static_cast<uint32_t>(branch_begin + 9);

            // If the jump crosses or touches 32-byte boundary, align it
            if ((branch_begin ^ branch_end) >= 32) {
                uint32_t alignment_size = 32 - (branch_begin & 31);
                if (alignment_size > 8) {
                    emit(NOPX[alignment_size - 9], alignment_size - 8, code, codePos);
                    alignment_size = 8;
                }
                emit(NOPX[alignment_size - 1], alignment_size, code, codePos);
            }
        }

        *(uint64_t*)(code + codePos) = 0x850f01eb83ull;
        codePos += 5;
        emit32(prologueSize - codePos - 4, code, codePos);
        emitByte(0xe9, code, codePos);
        emit32(epilogueOffset - codePos - 4, code, codePos);
    }

    template<bool AVX2>
    FORCE_INLINE void JitCompilerX86::generateSuperscalarCode(Instruction& instr, uint8_t* code, uint32_t& codePos) {
        switch ((SuperscalarInstructionType)instr.opcode)
        {
        case randomx::SuperscalarInstructionType::ISUB_R:
            *(uint32_t*)(code + codePos) = 0x00C02B4DUL + (instr.dst << 19) + (instr.src << 16);
            codePos += 3;
            if (AVX2) {
                emit32(0xC0FBFDC5UL + (instr.src << 24) + (instr.dst << 27) - (instr.dst << 11), code, codePos);
            }
            break;
        case randomx::SuperscalarInstructionType::IXOR_R:
            *(uint32_t*)(code + codePos) = 0x00C0334DUL + (instr.dst << 19) + (instr.src << 16);
            codePos += 3;
            if (AVX2) {
                emit32(0xC0EFFDC5UL + (instr.src << 24) + (instr.dst << 27) - (instr.dst << 11), code, codePos);
            }
            break;
        case randomx::SuperscalarInstructionType::IADD_RS:
            emit32(0x00048D4F + (instr.dst << 19) + (genSIB(instr.getModShift(), instr.src, instr.dst) << 24), code, codePos);
            if (AVX2) {
                if (instr.getModShift()) {
                    static const uint8_t t[] = { 0xC5, 0xBD, 0x73, 0xF0, 0x00, 0xC5, 0xBD, 0xD4, 0xC0 };
                    uint8_t* p = code + codePos;
                    emit(t, code, codePos);
                    p[3] += instr.src;
                    p[4] = instr.getModShift();
                    p[8] += instr.dst * 9;
                }
                else {
                    emit32(0xC0D4FDC5UL + (instr.src << 24) + (instr.dst << 27) - (instr.dst << 11), code, codePos);
                }
            }
            break;
        case randomx::SuperscalarInstructionType::IMUL_R:
            emit32(0xC0AF0F4DUL + (instr.dst << 27) + (instr.src << 24), code, codePos);
            if (AVX2) {
                static const uint8_t t[] = {
                    0xC5, 0xBD, 0x73, 0xD0, 0x20,
                    0xC5, 0xB5, 0x73, 0xD0, 0x20,
                    0xC5, 0x7D, 0xF4, 0xD0,
                    0xC5, 0x35, 0xF4, 0xD8,
                    0xC5, 0xBD, 0xF4, 0xC0,
                    0xC4, 0xC1, 0x25, 0x73, 0xF3, 0x20,
                    0xC5, 0xFD, 0x73, 0xF0, 0x20,
                    0xC4, 0x41, 0x2D, 0xD4, 0xD3,
                    0xC5, 0xAD, 0xD4, 0xC0
                };
                uint8_t* p = code + codePos;
                emit(t, code, codePos);
                p[3] += instr.dst;
                p[8] += instr.src;
                p[11] -= instr.dst * 8;
                p[13] += instr.src;
                p[17] += instr.dst;
                p[21] += instr.dst * 8 + instr.src;
                p[29] -= instr.dst * 8;
                p[31] += instr.dst;
                p[41] += instr.dst * 9;
            }
            break;
        case randomx::SuperscalarInstructionType::IROR_C:
            {
                const uint32_t shift = instr.getImm32() & 63;
                emit32(0x00C8C149UL + (instr.dst << 16) + (shift << 24), code, codePos);
                if (AVX2) {
                    static const uint8_t t[] = { 0xC5, 0xBD, 0x73, 0xD0, 0x00, 0xC5, 0xB5, 0x73, 0xF0, 0x00, 0xC4, 0xC1, 0x3D, 0xEB, 0xC1 };
                    uint8_t* p = code + codePos;
                    emit(t, code, codePos);
                    p[3] += instr.dst;
                    p[4] = shift;
                    p[8] += instr.dst;
                    p[9] = 64 - shift;
                    p[14] += instr.dst * 8;
                }
            }
            break;
        case randomx::SuperscalarInstructionType::IADD_C7:
        case randomx::SuperscalarInstructionType::IADD_C8:
        case randomx::SuperscalarInstructionType::IADD_C9:
            if (AVX2) {
                static const uint8_t t[] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x03, 0xC0, 0xC4, 0x62, 0x7D, 0x19, 0x05, 0xEC, 0xFF, 0xFF, 0xFF, 0xC4, 0xC1, 0x7D, 0xD4, 0xC0 };
                uint8_t* p = code + codePos;
                emit(t, code, codePos);
                *(uint64_t*)(p + 2) = signExtend2sCompl(instr.getImm32());
                p[12] += instr.dst * 8;
                p[24] -= instr.dst * 8;
                p[26] += instr.dst * 8;
            }
            else {
                *(uint32_t*)(code + codePos) = 0x00C08149UL + (instr.dst << 16);
                codePos += 3;
                emit32(instr.getImm32(), code, codePos);
            }
            break;
        case randomx::SuperscalarInstructionType::IXOR_C7:
        case randomx::SuperscalarInstructionType::IXOR_C8:
        case randomx::SuperscalarInstructionType::IXOR_C9:
            if (AVX2) {
                static const uint8_t t[] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x33, 0xC0, 0xC4, 0x62, 0x7D, 0x19, 0x05, 0xEC, 0xFF, 0xFF, 0xFF, 0xC4, 0xC1, 0x7D, 0xEF, 0xC0 };
                uint8_t* p = code + codePos;
                emit(t, code, codePos);
                *(uint64_t*)(p + 2) = signExtend2sCompl(instr.getImm32());
                p[12] += instr.dst * 8;
                p[24] -= instr.dst * 8;
                p[26] += instr.dst * 8;
            }
            else {
                *(uint32_t*)(code + codePos) = 0x00F08149UL + (instr.dst << 16);
                codePos += 3;
                emit32(instr.getImm32(), code, codePos);
            }
            break;
        case randomx::SuperscalarInstructionType::IMULH_R:
            *(uint32_t*)(code + codePos) = 0x00C08B49UL + (instr.dst << 16);
            codePos += 3;
            *(uint32_t*)(code + codePos) = 0x00E0F749UL + (instr.src << 16);
            codePos += 3;
            *(uint32_t*)(code + codePos) = 0x00C28B4CUL + (instr.dst << 19);
            codePos += 3;
            if (AVX2) {
                static const uint8_t t[] = {
                    0xC5, 0xBD, 0x73, 0xD0, 0x20,
                    0xC5, 0xB5, 0x73, 0xD0, 0x20,
                    0xC5, 0x7D, 0xF4, 0xD0,
                    0xC5, 0x3D, 0xF4, 0xD8,
                    0xC4, 0x41, 0x7D, 0xF4, 0xE1,
                    0xC4, 0xC1, 0x3D, 0xF4, 0xC1,
                    0xC4, 0xC1, 0x2D, 0x73, 0xD2, 0x20,
                    0xC4, 0x41, 0x25, 0xEF, 0xC6,
                    0xC4, 0x41, 0x25, 0xD4, 0xDC,
                    0xC4, 0x41, 0x25, 0xD4, 0xDA,
                    0xC4, 0x41, 0x25, 0xEF, 0xCE,
                    0xC4, 0x42, 0x3D, 0x37, 0xC1,
                    0xC4, 0x41, 0x3D, 0xDB, 0xC7,
                    0xC5, 0xBD, 0xD4, 0xC0,
                    0xC4, 0xC1, 0x25, 0x73, 0xD3, 0x20,
                    0xC5, 0xA5, 0xD4, 0xC0
                };
                uint8_t* p = code + codePos;
                emit(t, code, codePos);
                p[3] += instr.dst;
                p[8] += instr.src;
                p[11] -= instr.dst * 8;
                p[13] += instr.src;
                p[17] += instr.src;
                p[20] -= instr.dst * 8;
                p[27] += instr.dst * 8;
                p[67] += instr.dst * 9;
                p[77] += instr.dst * 9;
            }
            break;
        case randomx::SuperscalarInstructionType::ISMULH_R:
            *(uint32_t*)(code + codePos) = 0x00C08B49UL + (instr.dst << 16);
            codePos += 3;
            *(uint32_t*)(code + codePos) = 0x00E8F749UL + (instr.src << 16);
            codePos += 3;
            *(uint32_t*)(code + codePos) = 0x00C28B4CUL + (instr.dst << 19);
            codePos += 3;
            if (AVX2) {
                static const uint8_t t[] = {
                    0xC5, 0xBD, 0x73, 0xD0, 0x20,
                    0xC5, 0xB5, 0x73, 0xD0, 0x20,
                    0xC5, 0x7D, 0xF4, 0xD0,
                    0xC5, 0x3D, 0xF4, 0xD8,
                    0xC4, 0x41, 0x7D, 0xF4, 0xE1,
                    0xC4, 0x41, 0x3D, 0xF4, 0xE9,
                    0xC4, 0xC1, 0x2D, 0x73, 0xD2, 0x20,
                    0xC4, 0x41, 0x25, 0xEF, 0xC6,
                    0xC4, 0x41, 0x25, 0xD4, 0xDC,
                    0xC4, 0x41, 0x25, 0xD4, 0xDA,
                    0xC4, 0x41, 0x25, 0xEF, 0xCE,
                    0xC4, 0x42, 0x3D, 0x37, 0xC1,
                    0xC4, 0x41, 0x3D, 0xDB, 0xC7,
                    0xC4, 0x41, 0x15, 0xD4, 0xE8,
                    0xC4, 0xC1, 0x25, 0x73, 0xD3, 0x20,
                    0xC4, 0x41, 0x15, 0xD4, 0xC3,
                    0xC4, 0x41, 0x35, 0xEF, 0xC9,
                    0xC4, 0x62, 0x35, 0x37, 0xD0,
                    0xC4, 0x62, 0x35, 0x37, 0xD8,
                    0xC5, 0x2D, 0xDB, 0xD0,
                    0xC5, 0x25, 0xDB, 0xD8,
                    0xC4, 0x41, 0x3D, 0xFB, 0xC2,
                    0xC4, 0xC1, 0x3D, 0xFB, 0xC3
                };
                uint8_t* p = code + codePos;
                emit(t, code, codePos);
                p[3] += instr.dst;
                p[8] += instr.src;
                p[11] -= instr.dst * 8;
                p[13] += instr.src;
                p[17] += instr.src;
                p[20] -= instr.dst * 8;
                p[89] += instr.dst;
                p[94] += instr.src;
                p[98] += instr.src;
                p[102] += instr.dst;
                p[112] += instr.dst * 8;
            }
            break;
        case randomx::SuperscalarInstructionType::IMUL_RCP:
            *(uint32_t*)(code + codePos) = 0x0000B848UL;
            codePos += 2;
            emit64(randomx_reciprocal_fast(instr.getImm32()), code, codePos);
            emit32(0xC0AF0F4CUL + (instr.dst << 27), code, codePos);
            if (AVX2) {
                static const uint8_t t[] = {
                    0xC4, 0x62, 0x7D, 0x19, 0x25, 0xEB, 0xFF, 0xFF, 0xFF,
                    0xC5, 0xBD, 0x73, 0xD0, 0x20,
                    0xC4, 0xC1, 0x35, 0x73, 0xD4, 0x20,
                    0xC4, 0x41, 0x7D, 0xF4, 0xD4,
                    0xC5, 0x35, 0xF4, 0xD8,
                    0xC4, 0xC1, 0x3D, 0xF4, 0xC4,
                    0xC4, 0xC1, 0x25, 0x73, 0xF3, 0x20,
                    0xC5, 0xFD, 0x73, 0xF0, 0x20,
                    0xC4, 0x41, 0x2D, 0xD4, 0xD3,
                    0xC5, 0xAD, 0xD4, 0xC0
                };
                uint8_t* p = code + codePos;
                emit(t, code, codePos);
                p[12] += instr.dst;
                p[22] -= instr.dst * 8;
                p[28] += instr.dst;
                p[33] += instr.dst * 8;
                p[41] -= instr.dst * 8;
                p[43] += instr.dst;
                p[53] += instr.dst * 9;
            }
            break;
        default:
            UNREACHABLE;
        }
    }

    template void JitCompilerX86::generateSuperscalarCode<false>(Instruction&, uint8_t*, uint32_t&);
    template void JitCompilerX86::generateSuperscalarCode<true>(Instruction&, uint8_t*, uint32_t&);

    template<bool rax>
    FORCE_INLINE void JitCompilerX86::genAddressReg(const Instruction& instr, const uint32_t src, uint8_t* code, uint32_t& codePos) {
        *(uint32_t*)(code + codePos) = (rax ? 0x24808d41 : 0x24888d41) + (src << 16);

        constexpr uint32_t add_table = 0x33333333u + (1u << (RegisterNeedsSib * 4));
        codePos += (add_table >> (src * 4)) & 0xf;

        emit32(instr.getImm32(), code, codePos);
        if (rax) {
            emitByte(0x25, code, codePos);
        }
        else {
            *(uint32_t*)(code + codePos) = 0xe181;
            codePos += 2;
        }
        emit32(AddressMask[instr.getModMem()], code, codePos);
    }

    template void JitCompilerX86::genAddressReg<false>(const Instruction& instr, const uint32_t src, uint8_t* code, uint32_t& codePos);
    template void JitCompilerX86::genAddressReg<true>(const Instruction& instr, const uint32_t src, uint8_t* code, uint32_t& codePos);

    FORCE_INLINE void JitCompilerX86::genAddressRegDst(const Instruction& instr, uint8_t* code, uint32_t& codePos) {
        const uint32_t dst = static_cast<uint32_t>(instr.dst) << 16;
        *(uint32_t*)(code + codePos) = 0x24808d41 + dst;
        codePos += (dst == (RegisterNeedsSib << 16)) ? 4 : 3;

        emit32(instr.getImm32(), code, codePos);
        emitByte(0x25, code, codePos);

        const uint32_t mask1 = AddressMask[instr.getModMem()];
        const uint32_t mask2 = ScratchpadL3Mask;
        emit32((instr.mod < (StoreL3Condition << 4)) ? mask1 : mask2, code, codePos);
    }

    FORCE_INLINE void JitCompilerX86::genAddressImm(const Instruction& instr, uint8_t* code, uint32_t& codePos) {
        emit32(instr.getImm32() & ScratchpadL3Mask, code, codePos);
    }

    void JitCompilerX86::h_IADD_RS(const Instruction& instr) {
        uint32_t pos = codePos;
        uint8_t* const p = code + pos;

        const uint32_t dst = instr.dst;
        const uint32_t sib = (instr.getModShift() << 6) | (instr.src << 3) | dst;

        uint32_t k = 0x048d4f + (dst << 19);
        if (dst == RegisterNeedsDisplacement)
            k = 0xac8d4f;

        *(uint32_t*)(p) = k | (sib << 24);
        *(uint32_t*)(p + 4) = instr.getImm32();

        pos += ((dst == RegisterNeedsDisplacement) ? 8 : 4);

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IADD_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst;

        if (src != dst) {
            genAddressReg<true>(instr, src, p, pos);
            emit32(0x0604034c + (dst << 19), p, pos);
        }
        else {
            *(uint32_t*)(p + pos) = 0x86034c + (dst << 19);
            pos += 3;
            genAddressImm(instr, p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_ISUB_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;
        
        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst;

        if (src != dst) {
            *(uint32_t*)(p + pos) = 0xc02b4d + (dst << 19) + (src << 16);
            pos += 3;
        }
        else {
            *(uint32_t*)(p + pos) = 0xe88149 + (dst << 16);
            pos += 3;
            emit32(instr.getImm32(), p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_ISUB_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst;

        if (src != dst) {
            genAddressReg<true>(instr, src, p, pos);
            emit32(0x06042b4c + (dst << 19), p, pos);
        }
        else {
            *(uint32_t*)(p + pos) = 0x862b4c + (dst << 19);
            pos += 3;
            genAddressImm(instr, p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IMUL_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst;

        if (src != dst) {
            emit32(0xc0af0f4d + ((dst * 8 + src) << 24), p, pos);
        }
        else {
            *(uint32_t*)(p + pos) = 0xc0694d + (((dst << 3) + dst) << 16);
            pos += 3;
            emit32(instr.getImm32(), p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IMUL_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            genAddressReg<true>(instr, src, p, pos);
            *(uint64_t*)(p + pos) = 0x0604af0f4cull + (dst << 27);
            pos += 5;
        }
        else {
            emit32(0x86af0f4c + (dst << 27), p, pos);
            genAddressImm(instr, p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IMULH_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst;

        *(uint32_t*)(p + pos) = 0xc08b49 + (dst << 16);
        *(uint32_t*)(p + pos + 3) = 0xe0f749 + (src << 16);
        *(uint32_t*)(p + pos + 6) = 0xc28b4c + (dst << 19);
        pos += 9;

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IMULH_R_BMI2(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst;

        *(uint32_t*)(p + pos) = 0xC4D08B49 + (dst << 16);
        *(uint32_t*)(p + pos + 4) = 0xC0F6FB42 + (dst << 27) + (src << 24);
        pos += 8;

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IMULH_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            genAddressReg<false>(instr, src, p, pos);
            *(uint64_t*)(p + pos) = 0x0e24f748c08b49ull + (dst << 16);
            pos += 7;
        }
        else {
            *(uint64_t*)(p + pos) = 0xa6f748c08b49ull + (dst << 16);
            pos += 6;
            genAddressImm(instr, p, pos);
        }
        *(uint32_t*)(p + pos) = 0xc28b4c + (dst << 19);
        pos += 3;

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IMULH_M_BMI2(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            genAddressReg<false>(instr, src, p, pos);
            *(uint32_t*)(p + pos) = static_cast<uint32_t>(0xC4D08B49 + (dst << 16));
            *(uint64_t*)(p + pos + 4) = 0x0E04F6FB62ULL + (dst << 27);
            pos += 9;
        }
        else {
            *(uint64_t*)(p + pos) = 0x86F6FB62C4D08B49ULL + (dst << 16) + (dst << 59);
            *(uint32_t*)(p + pos + 8) = instr.getImm32() & ScratchpadL3Mask;
            pos += 12;
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_ISMULH_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        *(uint64_t*)(p + pos) = 0x8b4ce8f749c08b49ull + (dst << 16) + (src << 40);
        pos += 8;
        emitByte(0xc2 + 8 * dst, p, pos);

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_ISMULH_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            genAddressReg<false>(instr, src, p, pos);
            *(uint64_t*)(p + pos) = 0x0e2cf748c08b49ull + (dst << 16);
            pos += 7;
        }
        else {
            *(uint64_t*)(p + pos) = 0xaef748c08b49ull + (dst << 16);
            pos += 6;
            genAddressImm(instr, p, pos);
        }
        *(uint32_t*)(p + pos) = 0xc28b4c + (dst << 19);
        pos += 3;

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IMUL_RCP(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;
        
        uint64_t divisor = instr.getImm32();
        if (!isZeroOrPowerOf2(divisor)) {
            const uint32_t dst = instr.dst;

            const uint64_t reciprocal = randomx_reciprocal_fast(divisor);
            if (imul_rcp_storage_used < 16) {
                *(uint64_t*)(imul_rcp_storage) = reciprocal;
                *(uint64_t*)(p + pos) = 0x2444AF0F4Cull + (dst << 27) + (static_cast<uint64_t>(248 - imul_rcp_storage_used * 8) << 40);
                ++imul_rcp_storage_used;
                imul_rcp_storage += 11;
                pos += 6;
            }
            else {
                *(uint32_t*)(p + pos) = 0xb848;
                pos += 2;

                emit64(reciprocal, p, pos);

                emit32(0xc0af0f4c + (dst << 27), p, pos);
            }

            registerUsage[dst] = pos;
        }

        codePos = pos;
    }

    void JitCompilerX86::h_INEG_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t dst = instr.dst;
        *(uint32_t*)(p + pos) = 0xd8f749 + (dst << 16);
        pos += 3;

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IXOR_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            *(uint32_t*)(p + pos) = 0xc0334d + (((dst << 3) + src) << 16);
            pos += 3;
        }
        else {
            const uint64_t imm = instr.getImm32();
            *(uint64_t*)(p + pos) = (imm << 24) + 0xf08149 + (dst << 16);
            pos += 7;
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IXOR_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            genAddressReg<true>(instr, src, p, pos);
            emit32(0x0604334c + (dst << 19), p, pos);
        }
        else {
            *(uint32_t*)(p + pos) = 0x86334c + (dst << 19);
            pos += 3;
            genAddressImm(instr, p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IROR_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            *(uint64_t*)(p + pos) = 0xc8d349c88b41ull + (src << 16) + (dst << 40);
            pos += 6;
        }
        else {
            *(uint32_t*)(p + pos) = 0xc8c149 + (dst << 16);
            pos += 3;
            emitByte(instr.getImm32() & 63, p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_IROL_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t src = instr.src;
        const uint64_t dst = instr.dst;

        if (src != dst) {
            *(uint64_t*)(p + pos) = 0xc0d349c88b41ull + (src << 16) + (dst << 40);
            pos += 6;
        }
        else {
            *(uint32_t*)(p + pos) = 0xc0c149 + (dst << 16);
            pos += 3;
            emitByte(instr.getImm32() & 63, p, pos);
        }

        registerUsage[dst] = pos;
        codePos = pos;
    }

    void JitCompilerX86::h_ISWAP_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst;

        if (src != dst) {
            *(uint32_t*)(p + pos) = 0xc0874d + (((dst << 3) + src) << 16);
            pos += 3;
            registerUsage[dst] = pos;
            registerUsage[src] = pos;
        }

        codePos = pos;
    }

    void JitCompilerX86::h_FSWAP_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint64_t dst = instr.dst;

        *(uint64_t*)(p + pos) = 0x01c0c60f66ull + (((dst << 3) + dst) << 24);
        pos += 5;

        codePos = pos;
    }

    void JitCompilerX86::h_FADD_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        prevFPOperation = pos;

        const uint64_t dst = instr.dst % RegisterCountFlt;
        const uint64_t src = instr.src % RegisterCountFlt;

        *(uint64_t*)(p + pos) = 0xc0580f4166ull + (((dst << 3) + src) << 32);
        pos += 5;

        codePos = pos;
    }

    void JitCompilerX86::h_FADD_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        prevFPOperation = pos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst % RegisterCountFlt;

        genAddressReg<true>(instr, src, p, pos);
        *(uint64_t*)(p + pos) = 0x41660624e60f44f3ull;
        *(uint32_t*)(p + pos + 8) = 0xc4580f + (dst << 19);
        pos += 11;

        codePos = pos;
    }

    void JitCompilerX86::h_FSUB_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        prevFPOperation = pos;

        const uint64_t dst = instr.dst % RegisterCountFlt;
        const uint64_t src = instr.src % RegisterCountFlt;

        *(uint64_t*)(p + pos) = 0xc05c0f4166ull + (((dst << 3) + src) << 32);
        pos += 5;

        codePos = pos;
    }

    void JitCompilerX86::h_FSUB_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        prevFPOperation = pos;

        const uint32_t src = instr.src;
        const uint32_t dst = instr.dst % RegisterCountFlt;

        genAddressReg<true>(instr, src, p, pos);
        *(uint64_t*)(p + pos) = 0x41660624e60f44f3ull;
        *(uint32_t*)(p + pos + 8) = 0xc45c0f + (dst << 19);
        pos += 11;

        codePos = pos;
    }

    void JitCompilerX86::h_FSCAL_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const uint32_t dst = instr.dst % RegisterCountFlt;

        emit32(0xc7570f41 + (dst << 27), p, pos);

        codePos = pos;
    }

    void JitCompilerX86::h_FMUL_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        prevFPOperation = pos;

        const uint64_t dst = instr.dst % RegisterCountFlt;
        const uint64_t src = instr.src % RegisterCountFlt;

        *(uint64_t*)(p + pos) = 0xe0590f4166ull + (((dst << 3) + src) << 32);
        pos += 5;

        codePos = pos;
    }

    void JitCompilerX86::h_FDIV_M(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        prevFPOperation = pos;

        const uint32_t src = instr.src;
        const uint64_t dst = instr.dst % RegisterCountFlt;

        genAddressReg<true>(instr, src, p, pos);

        *(uint64_t*)(p + pos) = 0x0624e60f44f3ull;
        pos += 6;
        if (hasXOP) {
            *(uint64_t*)(p + pos) = 0xd0e6a218488full;
            pos += 6;
        }
        else {
            *(uint64_t*)(p + pos) = 0xe6560f45e5540f45ull;
            pos += 8;
        }
        *(uint64_t*)(p + pos) = 0xe45e0f4166ull + (dst << 35);
        pos += 5;

        codePos = pos;
    }

    void JitCompilerX86::h_FSQRT_R(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        prevFPOperation = pos;

        const uint32_t dst = instr.dst % RegisterCountFlt;

        emit32(0xe4510f66 + (((dst << 3) + dst) << 24), p, pos);

        codePos = pos;
    }

    void JitCompilerX86::h_CFROUND(const Instruction& instr) {
        uint8_t* const p = code;
        int32_t t = prevCFROUND;

        if (t > prevFPOperation) {
            if (vm_flags & RANDOMX_FLAG_AMD) {
                memcpy(p + t, NOP26, 26);
            }
            else {
                memcpy(p + t, NOP14, 14);
            }
        }

        uint32_t pos = codePos;
        prevCFROUND = pos;

        const uint32_t src = instr.src;

        *(uint32_t*)(p + pos) = 0x00C08B49 + (src << 16);
        const int rotate = (static_cast<int>(instr.getImm32() & 63) - 2) & 63;
        *(uint32_t*)(p + pos + 3) = 0x00C8C148 + (rotate << 24);

        if (vm_flags & RANDOMX_FLAG_AMD) {
            *(uint64_t*)(p + pos + 7) = 0x742024443B0CE083ULL;
            *(uint64_t*)(p + pos + 15) = 0x8900EB0414AE0F0AULL;
            *(uint32_t*)(p + pos + 23) = 0x202444;
            pos += 26;
        }
        else {
            *(uint64_t*)(p + pos + 7) = 0x0414AE0F0CE083ULL;
            pos += 14;
        }

        codePos = pos;
    }

    void JitCompilerX86::h_CFROUND_BMI2(const Instruction& instr) {
        uint8_t* const p = code;
        int32_t t = prevCFROUND;

        if (t > prevFPOperation) {
            if (vm_flags & RANDOMX_FLAG_AMD) {
                memcpy(p + t, NOP25, 25);
            }
            else {
                memcpy(p + t, NOP13, 13);
            }
        }

        uint32_t pos = codePos;
        prevCFROUND = pos;

        const uint64_t src = instr.src;

        const uint64_t rotate = (static_cast<int>(instr.getImm32() & 63) - 2) & 63;
        *(uint64_t*)(p + pos) = 0xC0F0FBC3C4ULL | (src << 32) | (rotate << 40);

        if (vm_flags & RANDOMX_FLAG_AMD) {
            *(uint64_t*)(p + pos + 6) = 0x742024443B0CE083ULL;
            *(uint64_t*)(p + pos + 14) = 0x8900EB0414AE0F0AULL;
            *(uint32_t*)(p + pos + 22) = 0x202444;
            pos += 25;
        }
        else {
            *(uint64_t*)(p + pos + 6) = 0x0414AE0F0CE083ULL;
            pos += 13;
        }

        codePos = pos;
    }

    template<bool jccErratum>
    void JitCompilerX86::h_CBRANCH(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        const int reg = instr.dst;
        int32_t jmp_offset = registerUsage[reg];

        // if it jumps over the previous FP instruction that uses rounding, treat it as if FP instruction happened now
        if (jmp_offset <= prevFPOperation) {
            prevFPOperation = pos;
        }

        jmp_offset -= pos + 16;

        if (jccErratum) {
            const uint32_t branch_begin = static_cast<uint32_t>(pos + 7);
            const uint32_t branch_end = static_cast<uint32_t>(branch_begin + ((jmp_offset >= -128) ? 9 : 13));

            // If the jump crosses or touches 32-byte boundary, align it
            if ((branch_begin ^ branch_end) >= 32) {
                const uint32_t alignment_size = 32 - (branch_begin & 31);
                jmp_offset -= alignment_size;
                emit(JMP_ALIGN_PREFIX[alignment_size], alignment_size, p, pos);
            }
        }

        *(uint32_t*)(p + pos) = 0x00c08149 + (reg << 16);
        const int shift = instr.getModCond();
        const uint32_t or_mask = (1UL << RandomX_ConfigurationBase::JumpOffset) << shift;
        const uint32_t and_mask = rotl32(~static_cast<uint32_t>(1UL << (RandomX_ConfigurationBase::JumpOffset - 1)), shift);
        *(uint32_t*)(p + pos + 3) = (instr.getImm32() | or_mask) & and_mask;
        *(uint32_t*)(p + pos + 7) = 0x00c0f749 + (reg << 16);
        *(uint32_t*)(p + pos + 10) = RandomX_ConfigurationBase::ConditionMask_Calculated << shift;
        pos += 14;

        if (jmp_offset >= -128) {
            *(uint32_t*)(p + pos) = 0x74 + (static_cast<uint32_t>(jmp_offset) << 8);
            pos += 2;
        }
        else {
            *(uint64_t*)(p + pos) = 0x840f + (static_cast<uint64_t>(jmp_offset - 4) << 16);
            pos += 6;
        }

        //mark all registers as used
        uint64_t* r = (uint64_t*) registerUsage;
        uint64_t k = pos;
        k |= k << 32;
        for (unsigned j = 0; j < RegistersCount / 2; ++j) {
            r[j] = k;
        }

        codePos = pos;
    }

    void JitCompilerX86::h_ISTORE(const Instruction& instr) {
        uint8_t* const p = code;
        uint32_t pos = codePos;

        genAddressRegDst(instr, p, pos);
        emit32(0x0604894c + (static_cast<uint32_t>(instr.src) << 19), p, pos);

        codePos = pos;
    }

    void JitCompilerX86::h_NOP(const Instruction& instr) {
        emitByte(0x90, code, codePos);
    }

    alignas(64) InstructionGeneratorX86 JitCompilerX86::engine[256] = {};

    template void JitCompilerX86::h_CBRANCH<false>(const Instruction&);
    template void JitCompilerX86::h_CBRANCH<true>(const Instruction&);
}