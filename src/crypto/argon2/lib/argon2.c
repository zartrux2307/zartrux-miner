

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#include "argon2.h"
#include "encoding.h"
#include "core.h"

/*
 * @param context A pointer to a structure with input parameters.
 * @param type Argon2 type.
 * @param memory A pre-allocated memory buffer.
 * @param memory_size The size of the pre-allocated buffer in bytes.
 * @return ARGON2_OK on success.
 */
int argon2_ctx_mem(argon2_context *context, argon2_type type, void *memory, size_t memory_size) {
    int result = ARGON2_OK;

    /* 1. Validate all inputs */
    result = validate_inputs(context);
    if (ARGON2_OK != result) {
        return result;
    }
    
    if (memory == NULL) {
         return ARGON2_MEMORY_ALLOCATION_ERROR;
    }
    
    if (memory_size < context->m_cost * 1024) {
        return ARGON2_MEMORY_TOO_LITTLE;
    }

    argon2_instance_t instance;
    /* 2. Setup instance */
    instance.memory = (uint8_t*)memory;
    instance.version = context->version;
    instance.passes = context->t_cost;
    instance.memory_blocks = context->m_cost;
    instance.segment_length = instance.memory_blocks / (context->lanes * ARGON2_SYNC_POINTS);
    instance.lane_length = instance.segment_length * ARGON2_SYNC_POINTS;
    instance.lanes = context->lanes;
    instance.threads = context->threads;
    instance.type = type;
    instance.print_internals = context->flags & ARGON2_FLAG_PRINT_INTERNALS;
    // Don't free memory, it's managed externally
    instance.free_memory_cbk = NULL; 
    instance.allocate_memory_cbk = NULL;


    /* 3. Initialization: Hashing inputs, filling first blocks */
    result = initialize(&instance, context);
    if (ARGON2_OK != result) {
        return result;
    }

    /* 4. Filling memory */
    result = fill_memory_blocks(&instance);
    if (ARGON2_OK != result) {
        return result;
    }

    /* 5. Finalization */
    finalize(context, &instance);

    return ARGON2_OK;
}

int argon2_ctx(argon2_context *context, argon2_type type) {
    return argon2_ctx_mem(context, type, NULL, 0);
}

const char *argon2_error_message(int error_code) {
    switch (error_code) {
    case ARGON2_OK:
        return "OK";
    case ARGON2_OUTPUT_PTR_NULL:
        return "Output pointer is NULL";
    case ARGON2_OUTPUT_TOO_SHORT:
        return "Output is too short";
    case ARGON2_OUTPUT_TOO_LONG:
        return "Output is too long";
    case ARGON2_PWD_TOO_SHORT:
        return "Password is too short";
    case ARGON2_PWD_TOO_LONG:
        return "Password is too long";
    case ARGON2_SALT_TOO_SHORT:
        return "Salt is too short";
    case ARGON2_SALT_TOO_LONG:
        return "Salt is too long";
    case ARGON2_AD_TOO_SHORT:
        return "Associated data is too short";
    case ARGON2_AD_TOO_LONG:
        return "Associated data is too long";
    case ARGON2_SECRET_TOO_SHORT:
        return "Secret is too short";
    case ARGON2_SECRET_TOO_LONG:
        return "Secret is too long";
    case ARGON2_TIME_TOO_SMALL:
        return "Time cost is too small";
    case ARGON2_TIME_TOO_LARGE:
        return "Time cost is too large";
    case ARGON2_MEMORY_TOO_LITTLE:
        return "Memory cost is too small";
    case ARGON2_MEMORY_TOO_MUCH:
        return "Memory cost is too large";
    case ARGON2_LANES_TOO_FEW:
        return "Too few lanes";
    case ARGON2_LANES_TOO_MANY:
        return "Too many lanes";
    case ARGON2_PWD_PTR_MISMATCH:
        return "Password pointer is NULL, but password length is not 0";
    case ARGON2_SALT_PTR_MISMATCH:
        return "Salt pointer is NULL, but salt length is not 0";
    case ARGON2_SECRET_PTR_MISMATCH:
        return "Secret pointer is NULL, but secret length is not 0";
    case ARGON2_AD_PTR_MISMATCH:
        return "Associated data pointer is NULL, but ad length is not 0";
    case ARGON2_MEMORY_ALLOCATION_ERROR:
        return "Memory allocation error";
    case ARGON2_FREE_MEMORY_CBK_NULL:
        return "The free memory callback is NULL";
    case ARGON2_ALLOCATE_MEMORY_CBK_NULL:
        return "The allocate memory callback is NULL";
    case ARGON2_INCORRECT_PARAMETER:
        return "Argon2_Context context is NULL";
    case ARGON2_INCORRECT_TYPE:
        return "There is no such version of Argon2";
    case ARGON2_OUT_PTR_MISMATCH:
        return "Output pointer mismatch";
    case ARGON2_THREADS_TOO_FEW:
        return "Not enough threads";
    case ARGON2_THREADS_TOO_MANY:
        return "Too many threads";
    case ARGON2_MISSING_ARGS:
        return "Missing arguments";
    case ARGON2_ENCODING_FAIL:
        return "Encoding failed";
    case ARGON2_DECODING_FAIL:
        return "Decoding failed";
    case ARGON2_THREAD_FAIL:
        return "Threading failure";
    case ARGON2_DECODING_LENGTH_FAIL:
        return "Some of encoded parameters are too long or too short";
    case ARGON2_VERIFY_MISMATCH:
        return "The password does not match the supplied hash";
    default:
        return "Unknown error code";
    }
}