#include <stdio.h>

int main() {
    volatile unsigned long long i = 0;
    while (1) {
        i++;
        if (i % 1000000000ULL == 0)
            printf("Running...\n");
    }
    return 0;
}
