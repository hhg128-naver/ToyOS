[BITS 64]

section .text
global LoadGDT
global LoadIDT
global LoadPageTable
global isr0, isr3, isr13, isr14
extern ExceptionHandler

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

; ISR Macros and Common Logic
%macro ISR_NOERR 1
isr%1:
    push qword 0      ; 더미 에러 코드
    push qword %1     ; 인터럽트 번호
    jmp isr_common
%endmacro

; ISR 매크로 (에러 코드가 있는 경우)
%macro ISR_ERR 1
isr%1:
    push qword %1     ; 인터럽트 번호
    jmp isr_common
%endmacro

ISR_NOERR 0           ; #DE
ISR_NOERR 3           ; #BP
ISR_ERR   13          ; #GP
ISR_ERR   14          ; #PF

isr_common:
    ; 레지스터 저장 (상태 보존)
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

    ; C 언어 핸들러 호출
    mov rdi, rsp      ; 스택 포인터를 인자로 전달 (Registers 구조체)
    call ExceptionHandler

    ; 레지스터 복원
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

    add rsp, 16       ; 에러 코드 및 인터럽트 번호 제거
    iretq             ; 인터럽트 복귀
