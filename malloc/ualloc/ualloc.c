#include "ualloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

BNode HeaderBoundary;

UINT64 StartAddrOfHeap;

// These bins manage free memory lines.
Bin FreeBins;

// header boundary in memory area which store headers.
BNode HeaderBoundary;

UINT64 StartAddrOfHeap;

// Lines number of current heap.
UINT32 LinesNoOfHeap = 0;

// initialize HeaderBoundary, StartAddrOfHeap and FreeBins
void ualloc_init() {

    HeaderBoundary = (BNode)sbrk(0);

    // Firstly, we should get some memory area for headers. 
	if (sbrk(BLOCK_SIZE * 50000) == (void *)-1) {
		printf("Allocated failed!\n");
		exit(1);
	}

    StartAddrOfHeap = (UINT64)sbrk(0);

    printf("Start address of heap: 0x%x\n", StartAddrOfHeap);
	printf("End address of heap: 0x%x\n", StartAddrOfHeap + 1024 * 128);

    // initialize bins
    for (int i = 0; i < SizeTypes; i++) {
        FreeBins[i].Lines = i + 1;
        FreeBins[i].FirstBlock = NULL;
        FreeBins[i].LastBlock = NULL;
    }
}

// Add a new data block at the top of heap, exit if things go wrong. 
// Return NULL if the current heap nearly reaches its max value. 
void ExtendHeap(BNode Last, UINT32 size) {
    // calculate Lines count
    UINT32 Lines = size / PCMLine;

    // There remains 128KB space for heap. 
	if (InitHeapSize - LinesNoOfHeap < Lines)
		return NULL;

    BNode b = HeaderBoundary;
	HeaderBoundary += 1;
    b->DataAddr = sbrk(0);

    if (sbrk(Lines * PCMLine) == (void *)-1) {
		printf("Failed extending heap!\n");
		exit(1);
	}

    // if extend success, increament LinesNoOfHeap and set BNode  
    LinesNoOfHeap += Lines;

    b->header = PACK(size, 1); // pack size and isFree
    b->NextNode = NULL;
    b->PrevNode = Last;
    if (Last)
		Last->NextNode = b;

}

void *ualloc(UINT32 size) {
    BNode b;
    UINT32 DataSize = ALIGN(size + sizeof(BNode));
    UINT32 Lines = DataSize / PCMLine;
    
    if (Lines > SizeTypes) {
#ifdef DEBUG
        printf("Try to get block from top chunk!\n");
#endif
    }
    
}