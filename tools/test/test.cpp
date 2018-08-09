#include <stdlib.h>

void array_test() {
    int array[1000];
    for (int i = 0; i < 10; i++) {
        array[i] = i;
    }
    int a = 2;
    int b = 1;
    int c = a + b;
}

int malloc_test() {
    void *p = malloc(4);
    free(p);
}

int main() {
    int a = 0;
    array_test();
    int z = 0;
    malloc_test();
} 

