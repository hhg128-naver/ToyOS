# ToyOS 현재 상태 점검 보고서 (Status Check Report) - 2026-04-24

## 1. 개요 (Overview)
본 문서는 ToyOS 프로젝트의 현재 구현 상태를 점검하고, `GEMINI.md` 및 `README.md` 등 프로젝트 문서와의 불일치 사항을 정정하기 위해 작성되었습니다.

## 2. 현재 아키텍처 및 구현 현황 (Current Architecture & Implementation)

### 🚀 핵심 아키텍처
- **Mode**: x86_64 Long Mode (64비트 전용)
- **Bootloader**: UEFI Native (GNU-EFI 사용)
- **Graphics**: GOP (Graphics Output Protocol) 기반 프레임버퍼 제어
- **Standard Library**: Newlib 통합 완료 (`printf`, `malloc` 지원)

### ✅ 완료된 마일스톤 (Completed Milestones)
1. **커널 엔트리 및 부팅**: UEFI 부트로더가 커널(ELF64)을 로드하고 `BootInfo`를 통해 메모리 맵과 프레임버퍼 정보를 전달함.
2. **그래픽 시스템**: PSF 폰트를 활용한 텍스트 렌더링 시스템 구현.
3. **CPU 환경 설정**: 64비트용 GDT, IDT 구축 및 예외 처리(Exception Handling).
4. **메모리 관리**: 
    - 물리 메모리 관리자(PMM) - 비트맵 기반
    - 가상 메모리 관리자(VMM) - 4단계 페이징
    - 커널 힙(Kernel Heap) 및 Newlib 연동
5. **멀티태스킹**: 타이머 인터럽트(PIT) 기반의 선점형 스케줄러(Preemptive Scheduler) 및 컨텍스트 스위칭 구현.
6. **유저 모드 및 시스템 콜 (Latest)**: 
    - Ring 3 전환 (`iretq` 방식 사용)
    - `syscall` 명령어를 통한 시스템 콜 인터페이스 구축
    - 유저 모드 테스트 태스크 실행 확인
7. **하드웨어 지원**: FPU(Floating Point Unit) 및 SSE 활성화 완료.

## 3. 문서 정정 및 보완 사항 (Corrections)

- **`GEMINI.md` 업데이트**: 기존에 잘못 기재되어 있던 "32비트 Multiboot" 관련 내용을 "64비트 UEFI"로 전면 정정했습니다.
- **개발 계획 업데이트**: `doc/ToyOS_Future_Development_Plan.md`에서 7단계(유저 모드)를 완료 상태로 변경하고, 다음 목표(VFS, ELF 로더)를 구체화했습니다.

## 4. 향후 과제 (Next Steps)

1. **유저 프로세스 격리**: 현재 모든 태스크가 커널의 페이지 테이블을 공유하고 있으나, 보안 및 안정성을 위해 각 유저 태스크별 독립된 PML4 페이지 테이블 할당이 필요합니다.
2. **가상 파일 시스템 (VFS)**: 디스크 I/O 추상화를 위한 VFS 계층 설계가 필요합니다.
3. **ELF 로더 (ELF Loader)**: 커널 내장 함수가 아닌, 실제 FAT32 파티션에 저장된 ELF64 실행 파일을 로드하여 유저 프로세스로 실행하는 기능을 구현해야 합니다.
4. **고급 쉘 (Shell)**: 사용자와의 실제 상호작용을 위한 명령어 인터프리터 구현이 예정되어 있습니다.

## 5. 결론 (Conclusion)
ToyOS는 현재 64비트 커널로서의 핵심 기능을 모두 갖추었으며, 특히 유저 모드 전환과 시스템 콜 기반이 마련되어 있어 본격적인 OS의 형태를 갖추기 시작했습니다. 다음 단계인 파일 시스템 및 ELF 로딩을 통해 '자립 가능한 OS'로의 발전이 기대됩니다.
