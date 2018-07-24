#pragma once

#include <vector>
using namespace std;

#define UINT16 short unsigned int
#define UINT32 unsigned int
#define UINT64 unsigned long long
#define	PCMLine 32
#define SizeTypes 32
#define InitHeapSize 128 * 1024 / 32
#define Generation 3

// Memory block header. I split memory block into
// two parts: header part and data part. These part
// are not continuous.
struct BlockNode {
	// Each block is composed of sevaral NVM lines.
	// Each NVM line is 32 bytes block.
	UINT32 Lines;

	// Set a flag to indicate whether the block is free.
	bool IsFree;

	// Set a flag to indicate which age range the block 
	// belong to. Here we set three age range:
	// 	0 --> Young
	// 	1 --> Middle
	// 	2 --> Old
	UINT16 age;

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

// Head node of each list(alias bin).
typedef struct {
	// Indicate the number of PCM lines in the memory block
	// of this list.
	UINT32 Lines;
	BNode FirstBlock;
	BNode LastBlock;
}HNode;

typedef HNode Bin[SizeTypes];

// Three generation: Young, Middle and Old. Here, Young generation
// represents memory lines that aren't allocated too many times, 
// which means these areas don't bear so many writes. And so on,
// Middle generation and Old generation have similar implications.

// These bins manage free memory lines.
Bin FreeBins[3];
// These bins manage allocated memory lines.
Bin UsedBins[3];

// Lines number of current heap.
UINT32 LinesNoOfHeap = 0;

// Initialize bins
void InitializeBins();

BNode ExtendHeap(BNode LastNode, UINT32 Lines);

BNode AllocateBlock(Bin FreeBin, Bin UsedBin, UINT32 Idx, UINT32 Lines);
void FreeBlock(Bin UsedBin, Bin FreeBin, BNode b);

BNode SplitBlock(BNode OrigBlock, UINT32 Lines);
BNode MergeBlock(vector<UINT64> Cands);

UINT32 CalculateIndex(UINT32 Lines);
UINT16 CalculateAge(void * StartAddr, UINT32 Lines);
void WearCount(void * StartAddr, UINT32 Lines);
BNode GetHeader(UINT64 DataAddr);

void *galloc(UINT32 Size);
void gfree(void * p);

void check();
