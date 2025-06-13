; randomx_patch_nasm.asm - NASM x64 para Linux/GCC/MinGW
section .text
global randomx_patch_apply

randomx_patch_apply:
    push    rbx

    mov     eax, 1
    cpuid
    test    ecx, 1 << 28
    jz      .no_avx

    vmovaps xmm0, [rel const_data]
    vaddps  xmm1, xmm0, xmm0
    vmovaps [rdi], xmm1           ; RDI: primer argumento (System V ABI)

    mov     eax, 1
    pop     rbx
    ret

.no_avx:
    xor     eax, eax
    pop     rbx
    ret

section .rodata align=16
const_data:
    dd 0x3f800000, 0x40000000, 0x40400000, 0x40800000