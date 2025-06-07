

#pragma once

// Increase it if some configs use more cache accesses
#define RANDOMX_CACHE_MAX_ACCESSES 16

// Increase it if some configs use larger superscalar latency
#define RANDOMX_SUPERSCALAR_MAX_LATENCY 256

// Increase it if some configs use larger cache
#define RANDOMX_CACHE_MAX_SIZE  268435456

// Increase it if some configs use larger dataset
#define RANDOMX_DATASET_MAX_SIZE  2181038080

// Increase it if some configs use larger programs
#define RANDOMX_PROGRAM_MAX_SIZE       280

// Increase it if some configs use larger scratchpad
#define RANDOMX_SCRATCHPAD_L3_MAX_SIZE      2097152
