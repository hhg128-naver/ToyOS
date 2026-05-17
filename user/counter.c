#include "libuser.h"

int main(int arg) {
    for (int i = 0; i <= 4; i++) {
        print("Counter: ");
        // 간단한 숫자 출력
        char num[2] = { '0' + i, '\0' };
        print(num);
        print("\n");
        sleep(500); // 약간의 딜레이
    }
    print("Counter Done!\n");
    return 0;
}
