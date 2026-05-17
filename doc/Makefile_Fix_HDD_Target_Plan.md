# Makefile run-uefi-hdd 타겟 수정 계획 (Makefile Fix for run-uefi-hdd)

`make run-uefi-hdd` 명령 실행 중 `hdd.img`를 마운트한 후 `EFI/BOOT` 디렉토리가 존재하지 않아 파일 복사에 실패하는 문제를 해결합니다.

## 사용자 리뷰 필요 (User Review Required)

> [!NOTE]
> 이 수정은 `hdd.img`가 이미 FAT 파일 시스템으로 포맷되어 있다고 가정합니다. 만약 이미지 자체가 포맷되어 있지 않다면 마운트 단계에서 실패할 수 있습니다.

## 제안된 변경 사항 (Proposed Changes)

### 빌드 시스템 (Build System)

#### [MODIFY] [Makefile](file:///f:/ToyOS/Makefile)
- `run-uefi-hdd` 타겟 내에서 `mnt/EFI/BOOT` 디렉토리를 생성하는 명령을 추가합니다.

```diff
 run-uefi-hdd: $(UEFI_TARGET) $(TARGET)
 	mkdir -p iso/EFI/BOOT
 	cp $(UEFI_TARGET) iso/EFI/BOOT/BOOTX64.EFI
 	cp $(TARGET) iso/kernel
 	sudo mount -o loop hdd.img mnt
+	sudo mkdir -p mnt/EFI/BOOT
 	sudo cp $(UEFI_TARGET) mnt/EFI/BOOT/BOOTX64.EFI
 	sudo cp $(TARGET) mnt/kernel
 	sudo umount mnt
```

## 검증 계획 (Verification Plan)

### 수동 검증 (Manual Verification)
- `make run-uefi-hdd` 명령을 실행하여 오류 없이 이미지가 업데이트되고 QEMU가 실행되는지 확인합니다.
