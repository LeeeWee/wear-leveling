#ifndef _UALLOC_H
#define _UALLOC_H


#define UINT16 short unsigned int
#define UINT32 unsigned int
#define UINT64 unsigned long long
#define	PCMLine 32
#define SizeTypes 32   /** count of bins size types, form PCMLine to PCMLine * SizeTypes, incremented by PCMLine */
#define InitHeapSize 128 * 1024 / 32
#define MAX_BLOCK_NUM 100000
#define LIMIT_WEARCOUNT 5000

// given a size, and return the integral multiple of PCMLine
#define ALIGN(size) (((size) + (PCMLine - 1)) & ~(PCMLine - 1))

// Pack a size and allocated bit into a word
#define PACK(size, alloc)  ((size) | (alloc))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Memory block header. I split memory block into
// two parts: header part and data part. These part
// are not continuous.
struct BlockNode {
	// the header is a package of size and isFree (size | isFree)
    // size: data size, need to be the integral multiple of PCMLine
    // isFree: a flag to indicate whether the block is free, 1 indicates is free and 0 indicates is used
	UINT64 header;

	// These metadatas are used for linking all free
	// block headers into various lists.
	struct BlockNode * NextNode;
	struct BlockNode * PrevNode;

	// This ptr points to those PCM lines who actually 
	// bear data writes.
	void * DataAddr;
};

#define BLOCK_SIZE sizeof(struct BlockNode)

typedef struct BlockNode * BNode;

// struct for data, metadata is the pointer that points to the related BlockNode
// data address is the address returned by ualloc
struct DATA {
	BNode metadata; 
	char data[1];
};

typedef struct DATA * DataPtr;

// Head node of each list(alias bin).
typedef struct {
	// Indicate the number of PCM lines in the memory block
	// of this list.
	UINT32 Lines;
	BNode FirstBlock;
	BNode LastBlock;
}HNode;

typedef HNode Bin[SizeTypes];

// Record allocated times of each PCM line
UINT32 wearcounts[MAX_BLOCK_NUM] = {0};

// Initialize
void ualloc_init();

void *ualloc(UINT32 Size);
void ufree(void * p);

#endif