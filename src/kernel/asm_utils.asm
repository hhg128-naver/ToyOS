[BITS 64]

section .data
global user_rsp_temp_array
user_rsp_temp_array: times 16 dq 0

section .text
global sLoadGDT
global LoadIDT
global LoadPageTable
global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7, isr8, isr9
global isr10, isr11, isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19
global isr20, isr21, isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29
global isr30, isr31
global irq32, irq33, irq44, irq48, irq255  ; IRQ 0 (Timer), IRQ 1 (Keyboard), IRQ 12 (Mouse), APIC Timer, Spurious
global outb, inb, outw, inw, insw
extern ExceptionHandler
extern InterruptHandler
extern current_kernel_stack_top_array
extern SyscallHandler

; sLoadGDT(struct GDTPtr *gdt_ptr)
; RDI: gdt_ptr 주소
sLoadGDT:
    lgdt [rdi]          ; GDT 로드

    ; 커널 데이터 세그먼트 (0x10) 설정
    mov ax, 0x10        ; 세그먼트 인덱스 2 (2 * 8 = 16 = 0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 커널 코드 세그먼트 (0x08)로 Far Return을 통한 점프
    pop rsi             ; Return Address 저장
    push 0x08           ; 새로운 CS (세그먼트 인덱스 1)
    push rsi            ; Return Address를 다시 스택에 삽입
    retfq               ; Far Return (CS 갱신)

; LoadTSS(uint16_t tss_selector)
global LoadTSS
LoadTSS:
    ltr di              ; Task Register에 TSS 세그먼트 선택자 로드
    ret

; LoadIDT(struct IDTPtr *idt_ptr)
LoadIDT:
    lidt [rdi]
    ret

; LoadPageTable(void* pml4_addr)
LoadPageTable:
    mov cr3, rdi    ; CR3 레지스터에 PML4 테이블 주소 로드
    ret

; GetCR3() -> void*
global GetCR3
GetCR3:
    mov rax, cr3
    ret

; 인터럽트 활성화/비활성화
global EnableInterrupts
EnableInterrupts:
    sti
    ret

global DisableInterrupts
DisableInterrupts:
    cli
    ret

global GetRFLAGS
GetRFLAGS:
    pushfq
    pop rax
    ret

; 제어 레지스터 조작 함수
global ReadCR0
ReadCR0:
    mov rax, cr0
    ret

global WriteCR0
WriteCR0:
    mov cr0, rdi
    ret

global ReadCR4
ReadCR4:
    mov rax, cr4
    ret

global WriteCR4
WriteCR4:
    mov cr4, rdi
    ret

global InitFPU
InitFPU:
    finit
    ret

; MSR 읽기/쓰기 함수
global WriteMSR
WriteMSR:
    mov rax, rsi        ; 가닥 64비트 값의 하위 32비트 (RAX)
    mov rdx, rsi
    shr rdx, 32         ; 가닥 64비트 값의 상위 32비트 (RDX)
    mov rcx, rdi        ; MSR 주소 (RCX)
    wrmsr
    ret

global ReadMSR
ReadMSR:
    mov rcx, rdi
    rdmsr               ; 결과는 EDX:EAX에 저장됨
    shl rdx, 32
    or rax, rdx         ; RAX에 64비트 결과 합침
    ret

; I/O 포트 제어 함수
outb:
    mov al, sil     ; RDI(port), RSI(data) -> AL로 데이터 복사
    mov dx, di      ; DX로 포트 번호 복사
    out dx, al
    ret

inb:
    mov dx, di
    in al, dx
    ret

outw:
    mov ax, si      ; RDI(port), RSI(data) -> AX로 데이터 복사
    mov dx, di      ; DX로 포트 번호 번호 복사
    out dx, ax
    ret

inw:
    mov dx, di
    in ax, dx
    ret

insw:
    push rdi
    push rcx
    mov rcx, rdx    ; RDX(count) -> RCX (DX를 덮어쓰기 전에 먼저 복사)
    mov dx, di      ; RDI(port) -> DX
    mov rdi, rsi    ; RSI(buffer) -> RDI
    cld
    rep insw
    pop rcx
    pop rdi
    ret

; ISR Macros
%macro ISR_NOERR 1
isr%1:
    push qword 0      ; 더미 에러 코드
    push qword %1     ; 인터럽트 번호
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    push qword %1     ; 인터럽트 번호
    jmp isr_common
%endmacro

; IRQ Macros (32번부터 시작)
%macro IRQ 1
irq%1:
    push qword 0      ; 더미 에러 코드
    push qword %1     ; 인터럽트 번호
    jmp irq_common
%endmacro

ISR_NOERR 0 ; #DE
ISR_NOERR 1 ; #DB
ISR_NOERR 2 ; NMI
ISR_NOERR 3 ; #BP
ISR_NOERR 4 ; #OF
ISR_NOERR 5 ; #BR
ISR_NOERR 6 ; #UD
ISR_NOERR 7 ; #NM
ISR_ERR   8 ; #DF
ISR_NOERR 9 ; Coprocessor Segment Overrun
ISR_ERR   10; #TS
ISR_ERR   11; #NP
ISR_ERR   12; #SS
ISR_ERR   13; #GP
ISR_ERR   14; #PF
ISR_NOERR 15; Reserved
ISR_NOERR 16; #MF
ISR_ERR   17; #AC
ISR_NOERR 18; #MC
ISR_NOERR 19; #XM
ISR_NOERR 20; #VE
ISR_ERR   21; Control Protection Exception
ISR_NOERR 22; Reserved
ISR_NOERR 23; Reserved
ISR_NOERR 24; Reserved
ISR_NOERR 25; Reserved
ISR_NOERR 26; Reserved
ISR_NOERR 27; Reserved
ISR_NOERR 28; Reserved
ISR_ERR   29; VMM Communication Exception
ISR_ERR   30; Security Exception
ISR_NOERR 31; Reserved

IRQ 32                ; IRQ 0 (Timer)
IRQ 33                ; IRQ 1 (Keyboard)
IRQ 44                ; IRQ 12 (Mouse)
IRQ 48                ; APIC Timer

; irq255: LAPIC Spurious Interrupt Vector (벡터 0xFF)
; Spurious 인터럽트는 EOI를 보내지 않고 바로 반환해야 합니다.
irq255:
    iretq

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call ExceptionHandler
    mov rsp, rax     ; 새로운 RSP 적용 (예외 처리 후 태스크 전환 가능성 대비)

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    o64 iret

irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call InterruptHandler
    mov rsp, rax     ; 스케줄러가 반환한 새로운 RSP로 전환

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    o64 iret

; SyscallEntry: syscall 명령어 진입점
global SyscallEntry
SyscallEntry:
    ; 1. 유저 스택에 rcx (유저 RIP) 백업
    push rcx

    ; 2. rcx를 임시 레지스터로 사용하여 CPU ID를 구함 (APIC ID)
    mov rcx, 0xFEE00020
    mov ecx, [rcx]
    shr ecx, 24             ; ecx = CPU ID (0 ~ 15)

    ; 3. 현재의 rsp (유저 스택 주소, rcx 백업됨)를 user_rsp_temp_array에 보관
    mov [rel user_rsp_temp_array + rcx*8], rsp

    ; 4. 커널 스택으로 전환
    mov rsp, [rel current_kernel_stack_top_array + rcx*8]

    ; 5. 시스템 콜 번호(rax)와 유저 RFLAGS(r11)를 커널 스택에 임시 보존
    push rax
    push r11

    ; 6. 유저 스택에 백업해 둔 원래 rcx (유저 RIP)를 읽어 rax에 보관
    mov r11, [rel user_rsp_temp_array + rcx*8]
    mov rax, [r11]          ; rax = 원래 유저 RIP

    ; 7. 커널 스택에 iretq 복귀 프레임 빌드
    push qword 0x1B         ; User SS (Index 3, RPL 3)
    
    mov r11, [rel user_rsp_temp_array + rcx*8]
    add r11, 8              ; 원래 유저 RSP로 보정
    push r11                ; User RSP

    mov r11, [rsp + 16]     ; 커널 스택의 임시 RFLAGS 복사
    push r11                ; User RFLAGS

    push qword 0x23         ; User CS (Index 4, RPL 3)
    push rax                ; User RIP

    ; 8. 범용 레지스터 저장 프레임 빌드 (첫 번째 push rax를 위해 임시 RAX 복사)
    mov r11, [rsp + 48]     ; 원래 유저 RAX (시스템 콜 번호)
    push r11                ; push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 9. C 핸들러 호출 준비
    mov rax, [rsp + 112]    ; 원래 유저 RAX (시스템 콜 번호)
    mov r9,  r8         
    mov r8,  r10        
    mov rcx, rdx        
    mov rdx, rsi        
    mov rsi, rdi        
    mov rdi, rax        
    call SyscallHandler

    ; 10. 결과값(RAX) 반영
    mov [rsp + 112], rax

    ; 11. 범용 레지스터 복원
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 12. iretq를 통한 복귀 (스택 상위의 임시 보존 데이터는 재진입 시 덮어쓰이므로 무시)
    o64 iret
