#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "basemalloc.h"

#define RAND_DOUBLE(min, max) ((double)rand())/((RAND_MAX/(max-min))+min)
#define RAND_INT(min, max) (rand()%(max-min+1)+min)

#define bool char
#define true 1
#define false 0

int main() {
    void *add_array[100];
    bool unfreed_array[100] = {false};
    for (int i = 0; i < 100; i++) {
        unsigned int size = RAND_INT(1, 100);
        void *p = ffmalloc(size);
        printf("address: %p, size: 0x%x\n", p, size);
        add_array[i] = p;
        unfreed_array[i] = 1;
        if (RAND_DOUBLE(0, 1) > 0.1) {
            int free_index = RAND_INT(0, i + 1);
            if (unfreed_array[free_index] == true) {
                printf("free_addr: %p\n", add_array[free_index]);
                basefree(add_array[free_index]);
                unfreed_array[free_index] = false;
            }
        }
    } 

    return 0;
}


