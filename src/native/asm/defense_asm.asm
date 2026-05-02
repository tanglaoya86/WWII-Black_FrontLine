

global xtea_decrypt_block
global crc32_asm
global get_peb_asm
global timing_check_asm

section .text

; void xtea_decrypt_block(uint32_t v[2], const uint32_t k[4])
xtea_decrypt_block:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 0x20
    
    mov     rsi, rcx
    mov     rdi, rdx
    
    mov     eax, [rsi]
    mov     ebx, [rsi + 4]
    
    mov     ecx, 0xC6EF3720
    mov     edx, 32
    
.delta_loop:
    sub     ebx, [rdi]
    mov     r8d, eax
    shl     r8d, 4
    add     r8d, [rdi + 8]
    mov     r9d, eax
    add     r9d, ecx
    xor     r8d, r9d
    mov     r9d, eax
    shr     r9d, 5
    add     r9d, [rdi + 12]
    xor     r8d, r9d
    sub     ebx, r8d
    
    sub     eax, [rdi + 4]
    mov     r8d, ebx
    shl     r8d, 4
    add     r8d, [rdi]
    mov     r9d, ebx
    add     r9d, ecx
    xor     r8d, r9d
    mov     r9d, ebx
    shr     r9d, 5
    add     r9d, [rdi + 4]
    xor     r8d, r9d
    sub     eax, r8d
    
    sub     ecx, 0x9E3779B9
    dec     edx
    jnz     .delta_loop
    
    mov     [rsi], eax
    mov     [rsi + 4], ebx
    
    add     rsp, 0x20
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; uint32_t crc32_asm(const uint8_t* data, size_t len, uint32_t seed)
crc32_asm:
    push    rbx
    push    rsi
    push    r12
    sub     rsp, 0x20
    
    mov     rsi, rcx
    mov     r12, rdx
    mov     ebx, r8d
    not     ebx
    
    xor     rcx, rcx
    
.crc_loop:
    cmp     rcx, r12
    jae     .crc_done
    
    xor     bl, byte [rsi + rcx]
    
    mov     edx, 8
    
.poly_loop:
    mov     eax, ebx
    shr     ebx, 1
    test    eax, 1
    jz      .skip_xor
    xor     ebx, 0xEDB88320
.skip_xor:
    dec     edx
    jnz     .poly_loop
    
    inc     rcx
    jmp     .crc_loop
    
.crc_done:
    mov     eax, ebx
    not     eax
    
    add     rsp, 0x20
    pop     r12
    pop     rsi
    pop     rbx
    ret

; void* get_peb_asm(void)
get_peb_asm:
    mov     rax, gs:[0x60]
    ret

; int timing_check_asm(uint64_t* baseline)
timing_check_asm:
    push    rbx
    sub     rsp, 0x20
    
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     rbx, rax
    
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    
    sub     rax, rbx
    sub     rax, [rcx]
    
    cmp     rax, 0x400
    seta    al
    movzx   eax, al
    
    add     rsp, 0x20
    pop     rbx
    ret
