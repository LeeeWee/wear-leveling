#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define DEBUG

#ifdef DEBUG
#define ASSERT(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "ASSERT failed at line %d in file %s\n", __LINE__, __FILE__); \
        } \
    } while (0)
#else
#define ASSERT(x)
#endif


int main() {
    int a = 0;
    if (!a) {
        perror("a equal 0");
    }

    char *b = NULL;

    ASSERT(b);

    fprintf(stderr, "failed at line %d in file %s\n",  __LINE__, __FILE__);

    printf("sizeof(int): %lu\n", sizeof(int));
    printf("sizeof(long): %lu\n", sizeof(long));
    printf("sizeof(long long): %lu\n", sizeof(long long));
    printf("sizeof(float): %lu\n", sizeof(float));
    printf("sizeof(double): %lu\n", sizeof(double));


    return 0;
}