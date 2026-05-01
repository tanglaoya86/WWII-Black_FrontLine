# RA2_PYandCPP_refactor
## 所以你写HTML干什么   先别搞README了
这是错误的因为HTML依赖于浏览器环境 除此之外RA2资源文件受到EA版权的保护
我查了下资源确实受版权保护，so我们名字不能叫这个了，资源也得全部重新弄
不然我们就要把它弄成mod
## 我们还是直接弄成独立游戏吧，直接写我们要的纳粹的那个（90%不侵权），名字我想一想待会改
里面的建造厂还可以叫建造厂，发电厂叫发电厂（通用名都可以）
但“光凌”“天启”这种专用名词不行
“超时空”这种可能侵权

## 我马上用不了电脑了
看asm
##自己看
; NASM x64 Windows A* Pathfinding
; Input: map.txt (1st line: width height, then grid with S start, E end, # obstacle)
; Output: movement sequence (U D L R) to console

global main
extern CreateFileA, ReadFile, WriteFile, GetStdHandle, CloseHandle, ExitProcess
extern GetCommandLineA, wsprintfA, lstrlenA, MultiByteToWideChar
extern HeapAlloc, HeapFree, GetProcessHeap
extern WriteConsoleA

%define STD_INPUT_HANDLE -10
%define STD_OUTPUT_HANDLE -11
%define GENERIC_READ 0x80000000
%define OPEN_EXISTING 3
%define FILE_ATTRIBUTE_NORMAL 0x80
%define INVALID_HANDLE_VALUE -1
%define MAX_MAP_SIZE 4096
%define MAX_OPEN 100000
%define HEAP_ZERO_MEMORY 8

section .data
    mapFile db "map.txt", 0
    fmt db "%d %d", 0
    outBuf db 1024 dup(0)
    dirChars db 'U', 'D', 'L', 'R'
    newline db 13, 10
    space db ' '
    strLen dq 0

section .bss
    width resd 1
    height resd 1
    map resb MAX_MAP_SIZE
    dist resd MAX_MAP_SIZE
    prev resd MAX_MAP_SIZE
    openSet resb MAX_OPEN
    openCount resd 1
    path resd MAX_MAP_SIZE
    pathLen resd 1
    hHeap resq 1

section .text
main:
    push rbp
    mov rbp, rsp
    sub rsp, 0x60
    ; get heap
    call GetProcessHeap
    mov [rel hHeap], rax

    ; open map file
    mov rcx, mapFile
    mov rdx, GENERIC_READ
    mov r8, 0
    mov r9, 0
    call CreateFileA
    mov r12, rax
    cmp rax, INVALID_HANDLE_VALUE
    je .exit

    ; read file
    sub rsp, 32
    mov rcx, r12
    lea rdx, [rel map]
    mov r8, MAX_MAP_SIZE
    lea r9, [rel strLen]
    mov qword [rsp+32], 0
    call ReadFile
    add rsp, 32

    ; close file
    mov rcx, r12
    call CloseHandle

    ; parse width height (first line)
    lea rcx, [rel map]
    lea rdx, [rel fmt]
    lea r8, [rel width]
    lea r9, [rel height]
    call sscanf
    ; skip first line (find newline)
    lea rsi, [rel map]
.skipLine:
    lodsb
    cmp al, 10
    jne .skipLine
    dec rsi
    ; now rsi points to map grid
    ; compute map size
    mov eax, [rel width]
    imul eax, [rel height]
    mov ebx, eax

    ; initialize dist array to -1 (0xFFFFFFFF)
    lea rdi, [rel dist]
    mov ecx, MAX_MAP_SIZE
    mov eax, 0xFFFFFFFF
    rep stosd

    ; find start and end
    xor r8, r8
    xor r9, r9
    mov ecx, [rel height]
.loop_y:
    push rcx
    mov ecx, [rel width]
.loop_x:
    mov al, [rsi]
    cmp al, 'S'
    je .found_start
    cmp al, 'E'
    je .found_end
    jmp .next
.found_start:
    mov [rel startIdx], r8d
    jmp .next
.found_end:
    mov [rel endIdx], r8d
.next:
    inc rsi
    inc r8d
    loop .loop_x
    pop rcx
    inc r9d
    cmp r9d, [rel height]
    jl .loop_y

    ; A* search
    ; openSet as linear array, we will use simple unsorted insert, find min f each pop
    mov dword [rel openCount], 0
    ; insert start
    mov edi, [rel startIdx]
    call add_open
    mov dword [dist + edi*4], 0

.search_loop:
    cmp dword [rel openCount], 0
    je .no_path
    ; pop min f
    call pop_min
    mov edi, eax         ; current index
    cmp edi, [rel endIdx]
    je .found_path

    ; expand neighbors
    ; compute x, y
    mov edx, 0
    mov eax, edi
    div dword [rel width]   ; eax=y, edx=x
    mov ebx, eax            ; y
    mov ecx, edx            ; x
    ; for each neighbor (up, down, left, right)
    ; up
    test ebx, ebx
    jz .skip_up
    lea esi, [edi - 1 * width]
    mov r8d, [map + esi]
    cmp r8b, '#'
    je .skip_up
    mov r9d, [dist + edi*4]
    inc r9d
    cmp r9d, [dist + esi*4]
    jge .skip_up
    mov [dist + esi*4], r9d
    mov [prev + esi*4], edi
    push rdi
    mov edi, esi
    call add_open
    pop rdi
.skip_up:
    ; down
    lea esi, [ebx + 1]
    cmp esi, [rel height]
    jge .skip_down
    lea esi, [edi + 1 * width]
    mov r8d, [map + esi]
    cmp r8b, '#'
    je .skip_down
    mov r9d, [dist + edi*4]
    inc r9d
    cmp r9d, [dist + esi*4]
    jge .skip_down
    mov [dist + esi*4], r9d
    mov [prev + esi*4], edi
    push rdi
    mov edi, esi
    call add_open
    pop rdi
.skip_down:
    ; left
    test ecx, ecx
    jz .skip_left
    lea esi, [edi - 1]
    mov r8d, [map + esi]
    cmp r8b, '#'
    je .skip_left
    mov r9d, [dist + edi*4]
    inc r9d
    cmp r9d, [dist + esi*4]
    jge .skip_left
    mov [dist + esi*4], r9d
    mov [prev + esi*4], edi
    push rdi
    mov edi, esi
    call add_open
    pop rdi
.skip_left:
    ; right
    lea esi, [ecx + 1]
    cmp esi, [rel width]
    jge .skip_right
    lea esi, [edi + 1]
    mov r8d, [map + esi]
    cmp r8b, '#'
    je .skip_right
    mov r9d, [dist + edi*4]
    inc r9d
    cmp r9d, [dist + esi*4]
    jge .skip_right
    mov [dist + esi*4], r9d
    mov [prev + esi*4], edi
    push rdi
    mov edi, esi
    call add_open
    pop rdi
.skip_right:
    jmp .search_loop

.found_path:
    ; reconstruct path
    mov dword [rel pathLen], 0
    mov edi, [rel endIdx]
    lea rsi, [rel path]
    mov ecx, 0
.reconstruct:
    mov [rsi + rcx*4], edi
    inc ecx
    cmp edi, [rel startIdx]
    je .done_reconstruct
    mov edi, [prev + edi*4]
    jmp .reconstruct
.done_reconstruct:
    mov [rel pathLen], ecx
    ; reverse to output moves
    lea rsi, [rel path]
    mov ebx, ecx
    dec ebx
    ; output sequence
    ; get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r12, rax
    ; loop from end to beginning, write direction
    lea rsi, [rel path]
    mov r8d, [rel pathLen]
    dec r8d
    mov r9, 0  ; output buffer index
.output_loop:
    cmp r8d, 0
    jl .done_output
    ; current pos
    mov edi, [rsi + r8*4]
    dec r8d
    cmp r8d, 0
    jl .done_output
    ; next pos
    mov esi_nxt, [rsi + r8*4]      ; next = path[r8]
    ; determine direction
    mov eax, edi
    sub eax, esi_nxt
    cmp eax, -1
    je .left_move
    cmp eax, 1
    je .right_move
    mov ebx, [rel width]
    neg ebx
    cmp eax, ebx
    je .up_move
    cmp eax, [rel width]
    je .down_move
    jmp .output_loop
.left_move:
    mov byte [outBuf + r9], 'L'
    jmp .append_space
.right_move:
    mov byte [outBuf + r9], 'R'
    jmp .append_space
.up_move:
    mov byte [outBuf + r9], 'U'
    jmp .append_space
.down_move:
    mov byte [outBuf + r9], 'D'
    jmp .append_space
.append_space:
    inc r9
    cmp r8d, 0
    jl .done_output
    mov byte [outBuf + r9], ' '
    inc r9
    jmp .output_loop
.done_output:
    ; add newline
    mov byte [outBuf + r9], 13
    inc r9
    mov byte [outBuf + r9], 10
    inc r9
    ; write to console
    mov rcx, r12
    lea rdx, [outBuf]
    mov r8, r9
    lea r9, [strLen]
    mov qword [rsp+32], 0
    call WriteFile
    jmp .exit

.no_path:
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r12, rax
    lea rdx, [noPathMsg]
    mov r8, noPathLen
    lea r9, [strLen]
    mov qword [rsp+32], 0
    call WriteFile

.exit:
    xor ecx, ecx
    call ExitProcess

; ---------- helper functions ----------
add_open:
    ; edi = index to add
    ; check if already in open set
    mov ecx, [rel openCount]
    lea rsi, [rel openSet]
    xor eax, eax
.add_check:
    cmp eax, ecx
    je .do_add
    cmp [rsi + rax], edi
    je .already
    inc eax
    jmp .add_check
.do_add:
    mov [rsi + rcx], edi
    inc dword [rel openCount]
.already:
    ret

pop_min:
    ; find index with minimal f = dist + heuristic (Manhattan)
    mov ecx, [rel openCount]
    test ecx, ecx
    jz .ret_invalid
    lea rsi, [rel openSet]
    xor r8, r8       ; best index in openSet
    mov r9d, [rsi]   ; best idx
    mov r10d, 0x7FFFFFFF  ; best f
    xor r11, r11     ; loop counter
.pop_loop:
    cmp r11d, ecx
    jge .pop_done
    mov edi, [rsi + r11*4]
    ; f = dist[edi] + heuristic(edi)
    mov eax, edi
    mov edx, 0
    div dword [rel width]  ; eax=y, edx=x
    ; manhattan to end
    mov ebx, [rel endIdx]
    mov ebp, 0
    div_start:
    ; compute end x,y
    mov esi, ebx
    mov edx, 0
    div dword [rel width]
    ; esi: endIdx, eax=endY, edx=endX
    sub eax, [rsp?]  ; need to properly compute
    ; simplify: call heuristic function
    push r11
    call heuristic
    pop r11
    add eax, [dist + edi*4]
    cmp eax, r10d
    jge .pop_next
    mov r10d, eax
    mov r8d, r11d
    mov r9d, edi
.pop_next:
    inc r11d
    jmp .pop_loop
.pop_done:
    ; remove r8 from openSet by swapping with last
    dec dword [rel openCount]
    mov ecx, [rel openCount]
    cmp r8d, ecx
    je .pop_ret
    mov eax, [rsi + rcx*4]
    mov [rsi + r8*4], eax
.pop_ret:
    mov eax, r9d
    ret
.ret_invalid:
    xor eax, eax
    ret

heuristic:
    ; edi = index, return Manhattan distance to endIdx
    push rbp
    mov rbp, rsp
    ; compute x,y for edi
    mov eax, edi
    mov edx, 0
    div dword [rel width]
    mov ebx, eax   ; y1
    mov ecx, edx   ; x1
    ; compute end x,y
    mov eax, [rel endIdx]
    mov edx, 0
    div dword [rel width]
    mov r8d, eax   ; y2
    mov r9d, edx   ; x2
    ; abs(y2-y1) + abs(x2-x1)
    sub r8d, ebx
    mov eax, r8d
    cdq
    xor eax, edx
    sub eax, edx
    mov ebx, eax
    sub r9d, ecx
    mov eax, r9d
    cdq
    xor eax, edx
    sub eax, edx
    add eax, ebx
    pop rbp
    ret

section .data
    noPathMsg db "No path found.", 13, 10
    noPathLen equ $ - noPathMsg
    startIdx dd 0
    endIdx dd 0
# 赶紧换一下目录换成我这个我已经在写ASM优化了
.
├── assets
│   ├── audio
│   │   ├── ambient
│   │   ├── speech
│   │   └── ui
│   ├── fonts
│   ├── images
│   │   ├── effects
│   │   ├── terrain
│   │   ├── ui
│   │   └── units
│   └── music
├── CMakeLists.txt
├── docs
│   └── README.md
├── LICENSE
├── logs_and_chat
│   ├── chat   #聊天
│   ├── debug
│   └── runtime
├── README.md
├── src
│   ├── native
│   │   ├── asm
│   │   │   ├── a_star_core.asm
│   │   │   ├── common.asm
│   │   │   └── pixel_copy.asm
│   │   └── cpp
│   │       ├── astar
│   │       │   ├── astar_types.h
│   │       │   ├── astar.cpp
│   │       │   └── astar.h
│   │       ├── bindings
│   │       │   ├── ctypes_exports.cpp
│   │       │   └── pybind11_bindings.cpp
│   │       └── render
│   │           ├── color_convert.cpp
│   │           ├── pixel_blitter.cpp
│   │           └── pixel_blitter.h
│   └── python
│       ├── __init__.py
│       ├── engine
│       │   ├── __init__.py
│       │   ├── audio.py
│       │   ├── input.py
│       │   └── render.py
│       ├── formats
│       │   ├── __init__.py
│       │   ├── mix_parser.py
│       │   ├── pal_parser.py
│       │   └── shp_parser.py
│       └── game
│           ├── __init__.py
│           ├── ai.py
│           ├── buildings.py
│           ├── rules.py
│           └── units.py
├── tests
│   ├── astar
│   │   └── test_astar.py
│   └── native_iface
│       └── test_native.py
└── tools
    ├── build_native.py
    └── extract_assets.py

33 directories, 33 files

