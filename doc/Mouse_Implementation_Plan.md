# 마우스 지원 구현 계획 (Mouse Support Implementation Plan)

이 문서는 ToyOS에서 PS/2 마우스를 지원하기 위한 구현 계획을 설명합니다.

## 1. 목표 (Goals)
- PS/2 마우스 드라이버 구현.
- IRQ 12 인터럽트를 통한 마우스 데이터 수신.
- 마우스 커서의 시각적 표시 및 이동 제어.

## 2. 세부 작업 (Detailed Tasks)

### 2.1. PS/2 마우스 초기화 (Initialization)
- PS/2 컨트롤러(0x64, 0x60 포트)를 통해 마우스를 활성화합니다.
- 보조 장치(마우스)를 활성화하고, 인터럽트 허용 설정을 수행합니다.
- 마우스에게 'Enable Data Reporting' 명령(0xF4)을 전송합니다.

### 2.2. 인터럽트 처리 (Interrupt Handling)
- `idt.c`에 IRQ 12(Vector 44) 핸들러를 등록합니다.
- `interrupt.c`에서 IRQ 12 발생 시 `Mouse_Handler`를 호출하도록 합니다.

### 2.3. 마우스 상태 관리 (State Management)
- 커널 내에 마우스의 현재 좌표(X, Y)와 버튼 상태를 유지하는 전역 구조체를 관리합니다.

### 2.4. 마우스 커서 렌더링 (Rendering)
- 프레임버퍼에 마우스 커서를 그리는 기능을 구현합니다.
