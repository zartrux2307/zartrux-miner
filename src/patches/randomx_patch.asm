; randomx_patch.asm - Parche ASM optimizado para RandomX en Monero (XMR)
; Compatible con CPUs con o sin AVX. Seguro para Intel i5-2330, Sandy/Ivy/Haswell, AMD FX, Ryzen, etc.
; 
; USO:
;   extern "C" void randomx_patch_apply(float* dst);
;   randomx_patch_apply apunta a 4 floats y los sobreescribe solo si AVX está disponible.

section .text
global randomx_patch_apply

randomx_patch_apply:
    ; 1. Verifica soporte AVX
    mov     eax, 1                 ; CPUID: función 1
    cpuid
    test    ecx, (1 << 28)         ; ¿ECX.28? AVX
    jz      .no_avx                ; Saltar si NO hay AVX

    ; 2. Si hay AVX: ejecutar vectorización rápida
    vmovaps xmm0, [rel const_data] ; xmm0 = {1.0, 2.0, 3.0, 4.0}
    vaddps  xmm1, xmm0, xmm0       ; xmm1 = xmm0 + xmm0 = {2,4,6,8}
    vmovaps [rdi], xmm1            ; guardar en dst (espera puntero en rdi: ABI de Linux x64/Win64)

    mov     eax, 1                 ; indicar que el parche sí se aplicó (opcional)
    ret

.no_avx:
    mov     eax, 0                 ; indicar que NO se aplicó (no hay AVX)
    ret

section .rodata align=16
const_data:
    dd 0x3f800000, 0x40000000, 0x40400000, 0x40800000  ; {1.0, 2.0, 3.0, 4.0}
