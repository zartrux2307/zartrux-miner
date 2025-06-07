; randomx_patch_masm.asm - MASM x64 para Windows/MSVC
_TEXT SEGMENT

; Constantes para el patch
const_data:
    REAL4 1.0, 2.0, 3.0, 4.0

; int randomx_patch_apply(float* dst)
public randomx_patch_apply
randomx_patch_apply proc
    push    rbx

    ; Verificar soporte AVX
    mov     eax, 1
    cpuid
    test    ecx, 10000000h        ; Bit 28 (AVX)
    jz      no_avx

    ; Cargar y procesar datos
    vmovaps xmm0, XMMWORD PTR [const_data]
    vaddps  xmm1, xmm0, xmm0
    vmovaps XMMWORD PTR [rcx], xmm1  ; dst[0..3] (RCX: primer argumento)

    mov     eax, 1
    pop     rbx
    ret

no_avx:
    xor     eax, eax
    pop     rbx
    ret
randomx_patch_apply endp

_TEXT ENDS
END