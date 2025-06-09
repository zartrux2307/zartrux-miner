#ifndef RANDOMX_H
#define RANDOMX_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "crypto/randomx/intrin_portable.h"

#define RANDOMX_HASH_SIZE 32
#define RANDOMX_DATASET_ITEM_SIZE 64

#ifndef RANDOMX_EXPORT
#define RANDOMX_EXPORT
#endif


enum randomx_flags {
  RANDOMX_FLAG_DEFAULT = 0,
  RANDOMX_FLAG_LARGE_PAGES = 1,
  RANDOMX_FLAG_HARD_AES = 2,
  RANDOMX_FLAG_FULL_MEM = 4,
  RANDOMX_FLAG_JIT = 8,
  RANDOMX_FLAG_1GB_PAGES = 16,
  RANDOMX_FLAG_AMD = 64,
};


struct randomx_dataset;
struct randomx_cache;
class randomx_vm;

// --- INICIO DE LA CORRECCIÓN ---
// Se deshabilita temporalmente la advertencia C4324 para MSVC.
// Esta advertencia informa sobre el relleno de la estructura para cumplir
// con la alineación, lo cual es esperado y seguro en este caso.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) 
#endif

struct RandomX_ConfigurationBase
{
    RandomX_ConfigurationBase();

    void Apply();

    // Common parameters for all RandomX variants
    enum Params : uint64_t
    {
        ArgonMemory = 262144,
        CacheAccesses = 8,
        SuperscalarMaxLatency = 170,
        DatasetBaseSize = 2147483648,
        DatasetExtraSize = 33554368,
        JumpBits = 8,
        JumpOffset = 8,
        CacheLineAlignMask_Calculated = (DatasetBaseSize - 1) & ~(RANDOMX_DATASET_ITEM_SIZE - 1),
        DatasetExtraItems_Calculated = DatasetExtraSize / RANDOMX_DATASET_ITEM_SIZE,
        ConditionMask_Calculated = ((1 << JumpBits) - 1) << JumpOffset,
    };

    uint32_t ArgonIterations;
    uint32_t ArgonLanes;
    const char* ArgonSalt;
    uint32_t SuperscalarLatency;

    uint32_t ScratchpadL1_Size;
    uint32_t ScratchpadL2_Size;
    uint32_t ScratchpadL3_Size;

    uint32_t ProgramSize;
    uint32_t ProgramIterations;
    uint32_t ProgramCount;

    uint32_t RANDOMX_FREQ_IADD_RS;
    uint32_t RANDOMX_FREQ_IADD_M;
    uint32_t RANDOMX_FREQ_ISUB_R;
    uint32_t RANDOMX_FREQ_ISUB_M;
    uint32_t RANDOMX_FREQ_IMUL_R;
    uint32_t RANDOMX_FREQ_IMUL_M;
    uint32_t RANDOMX_FREQ_IMULH_R;
    uint32_t RANDOMX_FREQ_IMULH_M;
    uint32_t RANDOMX_FREQ_ISMULH_R;
    uint32_t RANDOMX_FREQ_ISMULH_M;
    uint32_t RANDOMX_FREQ_IMUL_RCP;
    uint32_t RANDOMX_FREQ_INEG_R;
    uint32_t RANDOMX_FREQ_IXOR_R;
    uint32_t RANDOMX_FREQ_IXOR_M;
    uint32_t RANDOMX_FREQ_IROR_R;
    uint32_t RANDOMX_FREQ_IROL_R;
    uint32_t RANDOMX_FREQ_ISWAP_R;
    uint32_t RANDOMX_FREQ_FSWAP_R;
    uint32_t RANDOMX_FREQ_FADD_R;
    uint32_t RANDOMX_FREQ_FADD_M;
    uint32_t RANDOMX_FREQ_FSUB_R;
    uint32_t RANDOMX_FREQ_FSUB_M;
    uint32_t RANDOMX_FREQ_FSCAL_R;
    uint32_t RANDOMX_FREQ_FMUL_R;
    uint32_t RANDOMX_FREQ_FDIV_M;
    uint32_t RANDOMX_FREQ_FSQRT_R;
    uint32_t RANDOMX_FREQ_CBRANCH;
    uint32_t RANDOMX_FREQ_CFROUND;
    uint32_t RANDOMX_FREQ_ISTORE;
    uint32_t RANDOMX_FREQ_NOP;

    rx_vec_i128 fillAes4Rx4_Key[8];

    uint8_t codeSshPrefetchTweaked[20];
    uint8_t codePrefetchScratchpadTweaked[28];
    uint32_t codePrefetchScratchpadTweakedSize;

    uint32_t AddressMask_Calculated[4];
    uint32_t ScratchpadL3Mask_Calculated;
    uint32_t ScratchpadL3Mask64_Calculated;

#   if (XMRIG_ARM == 8)
    uint32_t Log2_ScratchpadL1;
    uint32_t Log2_ScratchpadL2;
    uint32_t Log2_ScratchpadL3;
    uint32_t Log2_DatasetBaseSize;
    uint32_t Log2_CacheSize;
#   endif
};

// Se restaura la configuración de advertencias a su estado original.
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
// --- FIN DE LA CORRECCIÓN ---

struct RandomX_ConfigurationMonero : public RandomX_ConfigurationBase {};
struct RandomX_ConfigurationWownero : public RandomX_ConfigurationBase { RandomX_ConfigurationWownero(); };
struct RandomX_ConfigurationArqma : public RandomX_ConfigurationBase { RandomX_ConfigurationArqma(); };
struct RandomX_ConfigurationGraft : public RandomX_ConfigurationBase { RandomX_ConfigurationGraft(); };
struct RandomX_ConfigurationSafex : public RandomX_ConfigurationBase { RandomX_ConfigurationSafex(); };
struct RandomX_ConfigurationYada : public RandomX_ConfigurationBase { RandomX_ConfigurationYada(); };

extern RandomX_ConfigurationMonero RandomX_MoneroConfig;
extern RandomX_ConfigurationWownero RandomX_WowneroConfig;
extern RandomX_ConfigurationArqma RandomX_ArqmaConfig;
extern RandomX_ConfigurationGraft RandomX_GraftConfig;
extern RandomX_ConfigurationSafex RandomX_SafexConfig;
extern RandomX_ConfigurationYada RandomX_YadaConfig;

extern RandomX_ConfigurationBase RandomX_CurrentConfig;

template<typename T>
void randomx_apply_config(const T& config)
{
    static_assert(sizeof(T) == sizeof(RandomX_ConfigurationBase), "Invalid RandomX configuration struct size");
    static_assert(std::is_base_of<RandomX_ConfigurationBase, T>::value, "Incompatible RandomX configuration struct");
    RandomX_CurrentConfig = config;
    RandomX_CurrentConfig.Apply();
}

void randomx_set_scratchpad_prefetch_mode(int mode);
void randomx_set_huge_pages_jit(bool hugePages);
void randomx_set_optimized_dataset_init(int value);

#if defined(__cplusplus)
extern "C" {
#endif

RANDOMX_EXPORT randomx_cache *randomx_create_cache(randomx_flags flags, uint8_t *memory);
RANDOMX_EXPORT void randomx_init_cache(randomx_cache *cache, const void *key, size_t keySize);
RANDOMX_EXPORT void randomx_release_cache(randomx_cache* cache);
RANDOMX_EXPORT randomx_dataset *randomx_create_dataset(uint8_t *memory);
RANDOMX_EXPORT unsigned long randomx_dataset_item_count(void);
RANDOMX_EXPORT void randomx_init_dataset(randomx_dataset *dataset, randomx_cache *cache, unsigned long startItem, unsigned long itemCount);
RANDOMX_EXPORT void *randomx_get_dataset_memory(randomx_dataset *dataset);
RANDOMX_EXPORT void randomx_release_dataset(randomx_dataset *dataset);
RANDOMX_EXPORT randomx_vm *randomx_create_vm(randomx_flags flags, randomx_cache *cache, randomx_dataset *dataset, uint8_t *scratchpad, uint32_t node);
RANDOMX_EXPORT void randomx_vm_set_cache(randomx_vm *machine, randomx_cache* cache);
RANDOMX_EXPORT void randomx_vm_set_dataset(randomx_vm *machine, randomx_dataset *dataset);
RANDOMX_EXPORT void randomx_destroy_vm(randomx_vm *machine);
RANDOMX_EXPORT void randomx_calculate_hash(randomx_vm *machine, const void *input, size_t inputSize, void *output);
RANDOMX_EXPORT void randomx_calculate_hash_first(randomx_vm* machine, uint64_t (&tempHash)[8], const void* input, size_t inputSize);
RANDOMX_EXPORT void randomx_calculate_hash_next(randomx_vm* machine, uint64_t (&tempHash)[8], const void* nextInput, size_t nextInputSize, void* output);

#if defined(__cplusplus)
}
#endif

#endif