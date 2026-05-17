# Shell C++ 리팩토링 결과

ToyOS의 대화형 쉘 기능을 기존 절차적 C 언어 방식에서 객체 지향적인 C++ 구조로 성공적으로 리팩토링하였습니다.

## 주요 변경 사항

### 1. 객체 지향 구조(OOP) 도입
기존의 `shell.c`는 `if-else` 분기문을 통해 명령어를 하드코딩하여 처리하고 있었습니다. 이를 다음과 같은 C++ 클래스 구조로 개선했습니다.

- **`Command` 인터페이스**: 모든 쉘 명령어가 공통으로 구현해야 하는 가상(virtual) 메서드(`getName`, `getDescription`, `execute`)를 정의했습니다.
- **개별 명령어 클래스**: `HelpCommand`, `ClearCommand`, `MemCommand`, `LsCommand`, `CatCommand`, `RebootCommand` 등 각 명령어마다 `Command` 인터페이스를 상속받은 전용 클래스를 만들어 책임을 분리했습니다.
- **`Shell` 클래스**: 사용자 입력을 받아 파싱하고, 등록된 명령어 목록(`commands` 배열)을 순회하며 일치하는 명령어의 `execute` 메서드를 호출하는 핵심 로직을 담당합니다.

### 2. 표준 라이브러리(STL) 미사용 설계
커널 환경에 풀 C++ 표준 라이브러리(`std::vector`, `std::string` 등)가 포팅되지 않은 상태임을 고려하여, 동적 메모리 할당(힙)을 남용하지 않고 **고정 크기 포인터 배열**(`Command* commands[MAX_COMMANDS]`)과 **C 스타일 문자열**(`char*`)을 사용하여 가볍고 빠르게 동작하도록 설계했습니다.

### 3. C/C++ 상호 연동
- 기존 커널 함수 호출(`kernel.c`, `pmm.c`, `vfs.c`)을 위해 `extern "C"` 블록을 사용하여 C 헤더 파일들을 안전하게 인클루드했습니다.
- 커널의 메인 함수(`kmain`)에서 C++ 쉘을 시작할 수 있도록 `Shell_Main()` 함수 자체를 `extern "C"`로 선언하여 링크 에러를 방지했습니다 (`shell.h` 포함).

## 빌드 검증 완료
기존 `shell.c`를 삭제하고 새 `shell.cpp`를 작성한 뒤 `make all`을 실행하였으며, 에러 없이 컴파일 및 링크가 정상 완료되었습니다.

## 확인 및 다음 단계
> [!TIP]
> 이제 명령어 추가가 매우 쉬워졌습니다. 새로운 기능을 추가할 때는 `Command` 클래스를 상속받는 새 클래스를 하나 만들고, `g_Shell.registerCommand()`로 등록하기만 하면 됩니다.

직접 `make run-uefi` 명령을 터미널에서 입력하여 기존 명령어(`ls`, `cat`, `mem`, `clear` 등)가 새 C++ 구조에서도 완벽히 동일하게 동작하는지 확인하시기 바랍니다.
