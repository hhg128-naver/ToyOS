#include <stdio.h>
#include <stdlib.h>

int main(int arg) {
    printf("Hello, Newlib World from ToyOS User Mode!\n");
    printf("Received argument: %d\n", arg);
    
    void* ptr = malloc(1024);
    if (ptr) {
        printf("Malloc success! Address: %p\n", ptr);
        free(ptr);
    } else {
        printf("Malloc failed!\n");
    }

    return 0;
}
