# VS Code + QEMU 디버깅 환경 설정 가이드

이 문서는 ToyOS의 64비트 UEFI 커널 및 부트로더를 VS Code에서 디버깅하는 방법을 안내합니다.

## 1. 사전 요구 사항 (Prerequisites)
- **GDB**: 시스템에 `gdb`가 설치되어 있어야 합니다. (WSL 사용 시 `sudo apt install gdb`)
- **OVMF**: UEFI 펌웨어 파일인 `OVMF.fd`가 `/usr/share/ovmf/OVMF.fd` 경로에 있어야 합니다.
- **VS Code Extension**: `C/C++` 확장 프로그램이 설치되어 있어야 합니다.

## 2. 주요 설정 요약 (Configuration Summary)
- **Makefile**: `CFLAGS`와 `EFI_CFLAGS`에 `-g` 플래그가 추가되어 디버그 심볼을 생성합니다.
- **tasks.json**: `run UEFI debug (QEMU)` 태스크가 QEMU를 `-s -S` 옵션으로 실행합니다.
- **launch.json**: GDB를 통해 `localhost:1234` 포트에 연결하고 `kernel` 파일의 심볼을 로드합니다.

## 3. 디버깅 시작하기 (Starting Debug Session)
1. **F5 키**를 누르거나, VS Code의 디버그 탭에서 **"QEMU GDB (Attach)"** 구성을 선택하고 실행합니다.
2. 실행 시 자동으로 다음 과정이 진행됩니다:
   - 커널 및 부트로더 빌드 (`make all`)
   - QEMU 실행 및 디버거 연결 대기 (`-s -S`)
   - VS Code 디버거가 GDB 서버에 연결
3. QEMU 창이 뜨지만 화면이 멈춰있는 것은 CPU가 중지된 상태이기 때문입니다. VS Code 상단의 **계속(Continue, F5)** 버튼을 누르면 커널 실행이 시작됩니다.

## 4. 중단점(Breakpoint) 설정
- 소스 코드(`src/kernel/kernel.c` 등)의 왼쪽 여백을 클릭하여 빨간색 중단점을 설정할 수 있습니다.
- 커널 실행 중 해당 라인에 도달하면 자동으로 실행이 멈추고 변수 값을 확인할 수 있습니다.

## 5. 부트로더(UEFI) 디버깅 전환
기본적으로 커널(`kernel`) 파일의 심볼을 로드하도록 설정되어 있습니다. UEFI 부트로더(`efi_main.c`)를 디버깅하려면 `.vscode/launch.json`의 `program` 경로를 다음과 같이 수정하세요:
```json
"program": "${workspaceFolder}/bootx64.so",
```
*주의: UEFI 부트로더는 실행 시점에 메모리 주소가 동적으로 할당되므로, 정확한 심볼 매핑을 위해서는 추가적인 `add-symbol-file` 명령이 필요할 수 있습니다.*

## 6. 문제 해결 (Troubleshooting)
- **포트 충돌**: 만약 `1234` 포트가 이미 사용 중이라면 QEMU가 실행되지 않을 수 있습니다. 실행 중인 QEMU 프로세스가 없는지 확인하세요.
- **심볼 로드 실패**: `Makefile`에서 `-g` 플래그가 정상적으로 적용되었는지, 빌드 결과물에 디버그 정보가 포함되어 있는지 확인하세요.
- **OVMF 경로 에러**: `Makefile`의 `run-uefi-debug` 타겟에서 `OVMF.fd` 경로가 실제 시스템과 일치하는지 확인하세요.

---
**Happy Debugging!** 🚀
