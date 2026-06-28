#include <stddef.h>

extern "C" {

typedef void (*constructor)();

extern constructor __init_array_start[];
extern constructor __init_array_end[];

/*
* 전역 및 정적(static) 객체의 생성자(Constructor)들을 순차적으로 실행하기 위해 사용하는 링커 심볼
* 컴파일러는 전역 객체의 생성자 호출 코드 주소들을 .init_array라는 특수한 섹션에 함수 포인터 배열 형태로 모아둡니다. 
* 링커 스크립트에서는 이 섹션의 앞과 뒤에 시작과 끝을 알리는 심볼을 배치합니다.
*/
void call_constructors() {
    for (constructor* i = __init_array_start; i != __init_array_end; i++) {
        (*i)();
    }
}

// virtual function pure virtual call stub
void __cxa_pure_virtual() {
    // Should kernel panic here
    while (1);
}

// Handle non-returning functions from standard library
int __cxa_atexit(void (*f)(void *), void *objptr, void *dso) {
    // We don't support exiting the kernel and destroying global objects gracefully yet
    return 0;
}

void __cxa_finalize(void *f) {
}

}

// new and delete operators
extern "C" {
    void* kmalloc(size_t);
    void kfree(void*);
}

void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* p) noexcept {
    kfree(p);
}

void operator delete(void* p, size_t size) noexcept {
    kfree(p);
}

void operator delete[](void* p) noexcept {
    kfree(p);
}

void operator delete[](void* p, size_t size) noexcept {
    kfree(p);
}
