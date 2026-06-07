;;; ap_trampoline.asm
;;; AP (Application Processor) 부팅 트램펄린 코드
;;;
;;; BSP가 이 코드를 물리 주소 0x8000에 복사한 뒤 INIT-SIPI-SIPI를 전송합니다.
;;; AP는 Real Mode (16비트)로 이 코드에서 깨어나며,
;;; Protected Mode를 거치지 않고 곧바로 64비트 Long Mode로 진입합니다.
;;;
;;; 빌드: nasm -f bin -o ap_trampoline.bin ap_trampoline.asm
;;;
;;; 공유 부팅 데이터 레이아웃 (AP_BOOT_DATA = 0x7F00):
;;;   오프셋 +0  (4바이트): pml4_phys    - CR3에 로드할 PML4 물리 주소
;;;   오프셋 +4  (4바이트): (패딩)
;;;   오프셋 +8  (8바이트): stack_top    - AP의 초기 커널 스택 최상단 주소
;;;   오프셋 +16 (8바이트): entry_point  - ap_entry() 함수의 64비트 가상 주소
;;;   오프셋 +24 (4바이트): booted_flag  - AP가 부팅 데이터 소비 완료 후 1로 설정

[BITS 16]
[ORG  0x8000]

AP_BOOT_DATA equ 0x7F00

ap_trampoline_start:
    cli
    cld

    ; 세그먼트 레지스터 초기화 (DS=0 → 물리 주소 직접 접근)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; 임시 GDT 로드 (이 트램펄린 바이너리 내부에 정의됨)
    lgdt [gdt64_ptr]

    ; CR3 ← BSP의 PML4 물리 주소 (4바이트, 하위 32비트만 사용)
    mov eax, dword [AP_BOOT_DATA + 0]
    mov cr3, eax

    ; CR4.PAE 활성화 (Long Mode 사용의 전제 조건)
    mov eax, cr4
    or  eax, (1 << 5)       ; PAE 비트
    mov cr4, eax

    ; IA32_EFER MSR에 LME(Long Mode Enable) 비트 설정
    mov ecx, 0xC0000080     ; IA32_EFER MSR 주소
    rdmsr
    or  eax, (1 << 8)       ; LME 비트
    wrmsr

    ; CR0.PE | CR0.PG 동시 설정 → Long Mode 활성화
    mov eax, cr0
    or  eax, (1 << 31) | (1 << 0)  ; PG | PE
    mov cr0, eax

    ; 64비트 코드 세그먼트(셀렉터 0x08)로 원거리 점프
    ; 16비트 코드에서 32비트 오퍼랜드 원거리 점프 명시적 인코딩:
    ;   0x66 = 오퍼랜드 크기 접두사 (32비트)
    ;   0xEA = far JMP 명령어
    ;   <32비트 절대 주소> <16비트 세그먼트 셀렉터>
    db 0x66
    db 0xEA
    dd ap_trampoline_64     ; 32비트 절대 주소 (ORG 0x8000 기준으로 계산됨)
    dw 0x08                 ; 커널 코드 세그먼트 (GDT 인덱스 1)

[BITS 64]
ap_trampoline_64:
    ; 데이터 세그먼트 레지스터 설정 (GDT 인덱스 2 = 0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax

    ; RSP ← 공유 부팅 데이터의 stack_top (AP 전용 커널 스택)
    mov rsp, qword [AP_BOOT_DATA + 8]

    ; 엔트리 포인트를 레지스터에 보관 (booted_flag 설정 후에도 유효)
    mov rax, qword [AP_BOOT_DATA + 16]

    ; booted_flag = 1 : BSP에게 이 AP가 부팅 데이터를 소비했음을 알림
    ; BSP는 이 플래그를 확인한 뒤 다음 AP의 데이터를 기록해도 안전함
    mov dword [AP_BOOT_DATA + 24], 1

    ; C 언어 엔트리 함수(ap_entry)로 점프 — 반환 없음
    jmp rax

; ============================================================
; 임시 GDT (트램펄린 전용)
; BSP의 GDT와 셀렉터 배치(0x08/0x10)를 동일하게 맞춰야 합니다.
; ap_entry()에서 InitGDT_AP()로 BSP의 정식 GDT를 로드하기 전까지 사용.
; ============================================================
align 8
gdt64:
    dq 0x0000000000000000   ; 0x00: NULL 디스크립터
    dq 0x00AF9A000000FFFF   ; 0x08: 64비트 커널 코드 (L=1, P=1, DPL=0)
    dq 0x00CF92000000FFFF   ; 0x10: 커널 데이터 (G=1, D=1, P=1, DPL=0)
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1   ; GDT 한계값 (바이트 수 - 1)
    dd gdt64                    ; GDT 베이스 물리 주소 (32비트, ORG 0x8000 기준)
