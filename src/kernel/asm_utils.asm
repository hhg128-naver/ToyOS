[BITS 64]

section .text
global LoadGDT
global LoadIDT
global LoadPageTable
global isr0, isr3, isr13, isr14
global irq32, irq33  ; IRQ 0 (Timer), IRQ 1 (Keyboard)
global outb, inb
extern ExceptionHandler
extern InterruptHandler

; LoadGDT(struct GDTPtr *gdt_ptr)
; RDI: gdt_ptr 주소
LoadGDT:
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

; LoadIDT(struct IDTPtr *idt_ptr)
LoadIDT:
    lidt [rdi]
    ret

; LoadPageTable(void* pml4_addr)
LoadPageTable:
    mov cr3, rdi    ; CR3 레지스터에 PML4 테이블 주소 로드
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

ISR_NOERR 0           ; #DE
ISR_NOERR 3           ; #BP
ISR_ERR   13          ; #GP
ISR_ERR   14          ; #PF

IRQ 32                ; IRQ 0 (Timer)
IRQ 33                ; IRQ 1 (Keyboard)

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
    iretq

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
    iretq
