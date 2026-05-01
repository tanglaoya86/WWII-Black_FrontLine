
; pixel_copy.asm - 快速像素复制 (SSE2, Windows x64 ABI)
; 导出函数: copy_pixels_sse2
; 调用约定: rcx = dest, rdx = src, r8 = num_bytes
; 非易失寄存器: rbx, rbp, rdi, rsi, r12-r15, xmm6-xmm15


global copy_pixels_sse2

section .text

copy_pixels_sse2:
    mov     rax, r8             
    test    rax, rax
    jz      .done

.loop_16:
    cmp     rax, 16
    jb      .tail
    movdqa  xmm0, [rdx]        ; 从 src 读 16 字节（需对齐，若未对齐改用 movdqu）
    movdqa  [rcx], xmm0        ; 写到 dest
    add     rdx, 16
    add     rcx, 16
    sub     rax, 16
    jmp     .loop_16

.tail:
    test    rax, rax
    jz      .done
.tail_loop:
    mov     r8b, [rdx]         ; r8b 临时借用（r8 已被占用，用其它寄存器，这里换成 sil 或 r9b 更好，为清晰换 r9）
    mov     [rcx], r8b
    inc     rdx
    inc     rcx
    dec     rax
    jnz     .tail_loop

.done:
    ret
然后我会

extern "C" void copy_pixels_sse2(void* dest, const void* src, size_t num_bytes);

NASM编译后（我来）链接生成 .pyd (Python 扩展) 或 .dll


你的 Python 就大概也许可能或许应该可以用 ctypes 调...

https://chat.deepseek.com/share/3rr4xws3kmg3h53uw4     看这个
是那个方案，方案1？
