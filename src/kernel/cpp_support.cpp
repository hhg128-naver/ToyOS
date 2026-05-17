#include <stddef.h>

extern "C" {

typedef void (*constructor)();

extern constructor __init_array_start[];
extern constructor __init_array_end[];

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
