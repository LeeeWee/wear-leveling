#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


typedef struct volatile_metadata {
    unsigned char state;
    uint32_t MWC; // metadata wear count 
    uint32_t DWC; // data wear count
} volatile_metadata;

int main() {

    printf("sizeof(volatile_metadata): %lu\n", sizeof(volatile_metadata));
    
    volatile_metadata **volatile_metadata_list;
    volatile_metadata_list = (volatile_metadata **)malloc(10 * sizeof(volatile_metadata *));
    // void *start_addr = (volatile_metadata *)malloc(10 * sizeof(volatile_metadata));

    printf("address of volatile_metadata_list: %p\n", volatile_metadata_list);
    for (int i = 0; i < 10; i++) {
        volatile_metadata_list[i] = (volatile_metadata *)malloc(sizeof(volatile_metadata));
        printf("address of volatile_metadata_list[%d]: %p\n", i, &volatile_metadata_list[i]);
        volatile_metadata_list[i]->state = 0;
        volatile_metadata_list[i]->DWC = i;
        volatile_metadata_list[i]->MWC = i;
    }
    for (int i = 0; i < 10; i++) {
        printf("address of volatile_metadata_list[%d]: %p, ", i, volatile_metadata_list[i]);
        printf("state: %d, DWC: %d, MWC: %d\n", volatile_metadata_list[i]->state, volatile_metadata_list[i]->DWC, volatile_metadata_list[i]->MWC);
    }
    
    volatile_metadata_list = (volatile_metadata **)realloc(volatile_metadata_list, 20 * sizeof(volatile_metadata *));

    for (int i = 0; i < 20; i++) {
        printf("address of volatile_metadata_list[%d]: %p\n", i, &volatile_metadata_list[i]);
    }

    for (int i = 0; i < 10; i++) {
        printf("address of volatile_metadata_list[%d]: %p, ", i, volatile_metadata_list[i]);
        printf("state: %d, DWC: %d, MWC: %d\n", volatile_metadata_list[i]->state, volatile_metadata_list[i]->DWC, volatile_metadata_list[i]->MWC);
    }

    int i = 1 / (64 / 64);
    printf("%d\n", i);

}