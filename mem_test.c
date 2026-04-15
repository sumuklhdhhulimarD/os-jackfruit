#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    size_t step = 5 * 1024 * 1024; // 5 MB
    while (1) {
        void *p = malloc(step);
        if (!p) break;
        printf("Allocated 5MB\n");
        sleep(1);
    }
    return 0;
}
