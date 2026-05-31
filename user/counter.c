#include <stdio.h>

int main(int arg) {
    for (int i = 0; i <= 4; i++) {
        printf("Counter: %d\n", i);
        // Simple delay loop
        for (volatile int j = 0; j < 50000000; j++);
    }
    printf("Counter Done!\n");
    return 0;
}
