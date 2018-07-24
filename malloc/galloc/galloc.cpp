#include <iostream>
#include "galloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <vector>
using namespace std;

// The map is used to record all headers of current data block
// in heap. Then we can use it to merge contiguous free PCM line. 
map<UINT64, BNode>CurHeaders;

// header boundary in memory area which store headers.
BNode HeaderBoundary;

UINT64 StartAddrOfHeap;

// Record allocated times of each PCM line. Then we can use it to
// guide movement of memory block between bins of different age.
UINT32 WearCounter[100000] = {0}; 

void InitializeBins() {

	HeaderBoundary = (BNode)sbrk(0);
	
	// Firstly, we should get some memory area for headers. Here,
	// we divide 64KB memory.
	if (sbrk(BLOCK_SIZE * 50000) == (void *)-1) {
		cout << "Allocated failed!" << endl;
		exit(1);
	}
	
	StartAddrOfHeap = (UINT64)sbrk(0);

	cout << "Start address of heap: 0x" << hex << StartAddrOfHeap << endl;
	cout << "End address of heap: 0x" << hex << (StartAddrOfHeap + 1024 * 128) << endl;

	int i, j;
	BNode Split;

	// In the beginning, all bins are empty.
	for (int k = 0; k < Generation; k++) {
		for (i = 0; i < SizeTypes; i++) {
			UINT32 Lines = i + 1;		
			FreeBins[k][i].Lines = Lines;
			FreeBins[k][i].FirstBlock = NULL;
			FreeBins[k][i].LastBlock = NULL;
			UsedBins[k][i].Lines = Lines;
			UsedBins[k][i].FirstBlock = NULL;
			UsedBins[k][i].LastBlock = NULL;
		}
	}
	/*
	// Only need to initialize Young FreeBin(FreeBin[0]). Pre-allocate
	// 5 memory blocks of each size.
	for (i = 0; i < SizeTypes; i++) {
		UINT32 Lines = i + 1;
		BNode Last = NULL;
		FreeBins[0][i].Lines = Lines;
		Split = ExtendHeap(Last, Lines);
		Last = Split;
		FreeBins[0][i].FirstBlock = Split;
		for (j = 1; j < 5; j++) {
			Split = ExtendHeap(Last, Lines);
			Last->NextNode = Split;
			Last = Split;
		}
		FreeBins[0][i].LastBlock = Split;
	}
*/
}

// Add a new data block at the top of heap, exit if things go wrong. 
// Return NULL if the current heap nearly reaches its max value. 
BNode ExtendHeap(BNode Last, UINT32 Lines) {
	BNode b;

	// If requested block is larger than 1KB(32 * 32), do it on the top of heap(top chunk). 
	if(Lines > 32)	{
		cout << "Cannot extend heap size that is larger than 32 PCM lines!" << endl;
		exit(1);
	}

	// There remains 128KB space for heap. 
	if (InitHeapSize - LinesNoOfHeap < Lines)
		return NULL;

	b = HeaderBoundary;
	HeaderBoundary += 1;
	b->DataAddr = sbrk(0);

	{
		cout << "Extend heap memory : " << dec << Lines << " lines" << endl;
		cout << "Header address at: 0x" << hex << (UINT64)b << endl;
		cout << "Data address at: 0x" << hex << (UINT64)b->DataAddr << endl;
		cout << endl;
	}

	if (sbrk(Lines * PCMLine) == (void *)-1) {
		cout << "Allocated failed!" << endl;
		exit(1);
	}

	LinesNoOfHeap += Lines;

	b->Lines = Lines;
	b->IsFree = 1;
	b->age = 0;
	b->NextNode = NULL;
	b->PrevNode = Last;
	if (Last)
		Last->NextNode = b;
	
	CurHeaders.insert(pair<UINT64, BNode>((UINT64)b->DataAddr, b));

	return b;
}

// When allocating block, move the first block header from free bin
// to used bin and return the address of allocated block header.
BNode AllocateBlock(Bin FreeBin, Bin UsedBin, UINT32 Idx, UINT32 Lines) {
	if(FreeBin[Idx].Lines < Lines) {
		cout << "Requested size is too large." << endl;
		exit(1);
	}

  	if(FreeBin[Idx].Lines == Lines) {
		// Select the first block in related free bin.
		BNode b = FreeBin[Idx].FirstBlock;
		if(b == NULL)	return b;

		FreeBin[Idx].FirstBlock = b->NextNode;
		if(b->NextNode == NULL)
			FreeBin[Idx].LastBlock = NULL;
		else
			b->NextNode->PrevNode = NULL;
		b->NextNode = NULL;
		b->IsFree = 0;
	
		// Move selected block to related used bin.
		if(UsedBin[Idx].FirstBlock == NULL) {
			UsedBin[Idx].FirstBlock = b;
			UsedBin[Idx].LastBlock = b;	
		}
		else {
			UsedBin[Idx].LastBlock->NextNode = b;
			b->PrevNode = UsedBin[Idx].LastBlock;
			UsedBin[Idx].LastBlock = b;
		}
		
		cout << "Get block from FreeBins[" << dec << b->age << "][" << dec << Idx << "]" << endl;
		cout << "First block in FreeBins[" << b->age << "][" << dec << Idx << "]: 0x" << hex << (UINT64)FreeBins[b->age][Idx].FirstBlock << endl;
		cout << "Last block in FreeBins[" << b->age << "][" << dec << Idx << "]: 0x" << hex << (UINT64)FreeBins[b->age][Idx].LastBlock << endl;
		cout << "Header address at: 0x" << hex << (UINT64)b << endl;
		cout << "Data address at: 0x" << hex << (UINT64)b->DataAddr << endl;
		cout << endl;
	
		// Finally we should increase wear count of each PCM line.
		WearCount(b->DataAddr, b->Lines);
		return b;	
	}
	
	// When allocator reach here, it means that we are searching larger bins.
	// So we must split block we get.

	// Select the first block in related free bin.
	BNode b = FreeBin[Idx].FirstBlock;
	if(b == NULL)	return b;

	cout << "Split larger block in FreeBins[" << dec << b->age << "][" << dec << Idx << "]" << endl;
	cout << "Start splitting from: 0x" << hex << (UINT64)b->DataAddr << endl; 

	b = SplitBlock(b, Lines);

	return b;
}

// When a memory block is allocated, each of its lines will increase their
// wear counter.
void WearCount(void * StartAddr, UINT32 Lines) {
	UINT64 Dist = (UINT64)StartAddr - StartAddrOfHeap;
	if(Dist % PCMLine != 0) {
		cout << "PCM line number must be an integer!" << endl;
		exit(1);
	}
	UINT32 LineNo = Dist / PCMLine;
	for (int i = 0; i < Lines; i++) 
		WearCounter[LineNo + i] += 1;
}

UINT16 CalculateAge(void * StartAddr, UINT32 Lines) {
	UINT64 Dist = (UINT64)StartAddr - StartAddrOfHeap;
	if(Dist % PCMLine != 0) {
		cout << "PCM line number must be an integer!" << endl;
		exit(1);
	}
	UINT32 LineNo = Dist / PCMLine;
	UINT32 TotalWear = 0, AverWear;
	UINT16 Age = 0;
	for (int i = 0; i < Lines; i++) 
		TotalWear += WearCounter[LineNo + i];
	AverWear = TotalWear / Lines;
	if(AverWear <= 50)
		Age = 0;
	else if(AverWear > 50 && AverWear <= 90)
		Age = 1;
	else
		Age = 2;
	return Age;
}

// Split original block to a block who has "Lines" PCM lines and a remainder block.
// The former block should be used to allocate. 
BNode SplitBlock(BNode OrigBlock, UINT32 Lines) {

	cout << "Original size: " << dec << OrigBlock->Lines << " lines" << endl;
	cout << "requested size: " << dec << Lines << " lines" << endl;

	if(OrigBlock->Lines <= Lines) {
		cout << "Original block must be larger than requested block!" << endl;
		exit(1);
	}

	BNode Remainder = HeaderBoundary;
	HeaderBoundary += 1;
	Remainder->Lines = OrigBlock->Lines - Lines;
	Remainder->IsFree = 1;
	Remainder->DataAddr = OrigBlock->DataAddr + Lines * PCMLine;
	Remainder->age = CalculateAge(Remainder->DataAddr, Remainder->Lines);
	Remainder->PrevNode = NULL;
	Remainder->NextNode = NULL;

	// Get age and index of target bin. Then insert remainder block to head of target bin
	// so that the fragment can be allocated firstly.
	UINT16 RemainderAge = Remainder->age;
	UINT32 RemainderIdx = Remainder->Lines - 1;
	if(FreeBins[RemainderAge][RemainderIdx].FirstBlock) {
		Remainder->NextNode = FreeBins[RemainderAge][RemainderIdx].FirstBlock;
		FreeBins[RemainderAge][RemainderIdx].FirstBlock->PrevNode = Remainder;
		FreeBins[RemainderAge][RemainderIdx].FirstBlock = Remainder;
	}
	// When target bin is empty.
	else {
		FreeBins[RemainderAge][RemainderIdx].FirstBlock = Remainder;
		FreeBins[RemainderAge][RemainderIdx].LastBlock = Remainder;
	}
		
	CurHeaders.insert(pair<UINT64, BNode>((UINT64)Remainder->DataAddr, Remainder));
		
	// If age is a huge number, the original block is a merged block so that it's not from bins.
	// Then there is no need to remove it from bins. 
	if(OrigBlock->age != 0xFFFF) {
		// Remove the newly-splited block from original bin.
		UINT16 OrigAge = OrigBlock->age;
		UINT32 OrigIdx = OrigBlock->Lines - 1;
		FreeBins[OrigAge][OrigIdx].FirstBlock = OrigBlock->NextNode;

		if(OrigBlock->NextNode == NULL)
			FreeBins[OrigAge][OrigIdx].LastBlock = NULL;
		else
			OrigBlock->NextNode->PrevNode = NULL;

		cout << "Original block header: 0x" << hex << (UINT64)OrigBlock << " size: " << dec << OrigBlock->Lines << " lines" 
			 << " from FreeBins[" << OrigAge << "][" << OrigIdx << "]" << endl;
		cout << "Data address : 0x" << hex << (UINT64)OrigBlock->DataAddr << endl;
	}
	
	if(OrigBlock->age == 0xFFFF) {
		cout << "Merged block header: 0x" << hex << (UINT64)OrigBlock << " size: " << dec << Lines << " lines" << endl;
		cout << "Data address : 0x" << hex << (UINT64)OrigBlock->DataAddr << endl;	
	}

	// Now we need to modify OrigBlock.
	OrigBlock->Lines = Lines;
	OrigBlock->IsFree = 0;
	OrigBlock->age = CalculateAge(OrigBlock->DataAddr, OrigBlock->Lines);
	OrigBlock->NextNode = NULL;
	OrigBlock->PrevNode = NULL;
	
	// Move newly-splitted block to related used bin.
	UINT16 NewAge = OrigBlock->age;
	UINT32 NewIdx = OrigBlock->Lines - 1;
	if(UsedBins[NewAge][NewIdx].LastBlock == NULL) {
		UsedBins[NewAge][NewIdx].FirstBlock = OrigBlock;
		UsedBins[NewAge][NewIdx].LastBlock = OrigBlock;
	}
	else {
		UsedBins[NewAge][NewIdx].LastBlock->NextNode = OrigBlock;
		OrigBlock->PrevNode = UsedBins[NewAge][NewIdx].LastBlock;
		UsedBins[NewAge][NewIdx].LastBlock = OrigBlock;
	}

	// Finally we should increase wear count of each PCM line.
	WearCount(OrigBlock->DataAddr, OrigBlock->Lines);
	
	cout << "Splitted block: 0x" << hex << (UINT64)OrigBlock << " size: " << dec << OrigBlock->Lines << " lines" 
		 << " to UsedBins[" << dec << NewAge << "][" << dec << NewIdx << "]" << endl;
	cout << "Data address : 0x" << hex << (UINT64)OrigBlock->DataAddr << endl;
	cout << "Remainder block: 0x" << hex << (UINT64)Remainder << " size: " << dec << Remainder->Lines << " lines" 
		 << " to FreeBins[" << dec << RemainderAge << "][" << dec << RemainderIdx << "]" << endl;
	cout << "Data address : 0x" << hex << (UINT64)Remainder->DataAddr << endl;	
	cout << endl;

	return OrigBlock;
}

void *galloc(UINT32 Size){	

	BNode b;	
	UINT32 Lines = (Size % PCMLine == 0) ? Size / PCMLine : Size / PCMLine + 1;
	UINT32 Idx = Lines - 1;
	
	// If index is larger than SizeTypes, then we should get block from top chunk.
	if(Idx >= SizeTypes) {
		cout << "Try to get block from top chunk!" << endl;
		exit(1);
	}

	cout << "Start galloc " << dec << Lines << " lines:" << endl; 

	for (int k = 0; k < Generation; k++) {
		// Firstly, we search related bin for block. If the bin is not empty,
		// we just pick the first block in bin.
		b = AllocateBlock(FreeBins[k], UsedBins[k], Idx, Lines);
		if(b)	return b->DataAddr;
		
		// If the bin we search is empty and heap size doesn't reach its max, 
		// then we try to extend heap for block.
		if(k == 0) {
			b = ExtendHeap(NULL, Lines);
			if(b) {
				UINT32 Idx = b->Lines - 1;

				// Move extended block to the tail of young used bin.
				if(UsedBins[0][Idx].FirstBlock == NULL) {
					UsedBins[0][Idx].FirstBlock = b;
					UsedBins[0][Idx].LastBlock = b;	
				}
				else {
					UsedBins[0][Idx].LastBlock->NextNode = b;
					b->PrevNode = UsedBins[0][Idx].LastBlock;
					UsedBins[0][Idx].LastBlock = b;
				}

				cout << "Move extended block to the tail of UsedBins[0][" << dec << Idx << "]." << endl; 
				
				WearCount(b->DataAddr, b->Lines);
		
				return b->DataAddr;
			}
		}

		// If heap size reach its max and the bin we search is empty, then we 
		// try to search for bins whose size is larger and split such large block.
		for(int i = Idx + 1; i < SizeTypes; i++) {
			b = AllocateBlock(FreeBins[k], UsedBins[k], i, Lines);
			if(b)	return b->DataAddr;
		}
		
		// If allocator reaches here, it means that there are no available blocks for
		// allocation in current age. Then we should try to merge contiguous block so 
		// that we can get a larger block for allocation.
		
		// Store the candidate merged blocks to a vector. Candidate merged blocks are 
		// free blocks which are contiguous and have same age. 
		vector<UINT64> MergeCands;
		UINT64 TotalLines = 0;
		map<UINT64, BNode>::iterator iter;
		for(iter = CurHeaders.begin(); iter != CurHeaders.end(); iter++) {
			// If the flag 'IsFree' is zero, current block has been allocated.
			if(iter->second->IsFree == 0 || iter->second->age != k) {
				MergeCands.clear();
				TotalLines = 0;
				continue;
			}
			
			// If age of current block is not the same as back block age of candidate vector,
			// these block cannot be merged.
			if(!MergeCands.empty())
				if(CurHeaders[MergeCands.back()]->age != iter->second->age) {
					MergeCands.clear();
					TotalLines = 0;
					continue;
				}
			
			TotalLines += iter->second->Lines;
			MergeCands.push_back(iter->first);
			
			if(TotalLines > Lines)	break;
		}
		if(TotalLines > Lines) {
			b = MergeBlock(MergeCands);
			b = SplitBlock(b, Lines);
			if(b)	return b->DataAddr;
		}
	}

	cout << "Sorry, there are no available blocks for such request." << endl;	
	exit(1);
}

BNode MergeBlock(vector<UINT64> Cands) {
	
	cout << "Start merging: " << endl;
	
	BNode MergedBlock = HeaderBoundary;
	HeaderBoundary += 1;

	vector<UINT64>::iterator iter = Cands.begin();
	
	MergedBlock->Lines = 0;
	MergedBlock->IsFree = 1;
	MergedBlock->age = 0xffff;	// Age is invalid in merged block. Because it will be splitted soon.
	MergedBlock->DataAddr = CurHeaders[*iter]->DataAddr;

	for(; iter != Cands.end(); iter++) {
		BNode b = CurHeaders[*iter];
		UINT16 CandAge = b->age;
		UINT32 CandIdx = b->Lines - 1;
		
		cout << "Sub block: 0x" << hex << (UINT64)b << " size: " << dec << b->Lines << " lines" << endl; 
		cout << "Data address : 0x" << hex << (UINT64)b->DataAddr << endl;

		// Remove such block from related free bin.

		// The block is the only block in used bin.
		if(b->PrevNode == NULL && b->NextNode == NULL) {
   			FreeBins[CandAge][CandIdx].FirstBlock = NULL;
   			FreeBins[CandAge][CandIdx].LastBlock = NULL;
    	}
		// The block is the first block but not the last block.
    	else if(b->PrevNode == NULL && b->NextNode != NULL) {
    	  	FreeBins[CandAge][CandIdx].FirstBlock = b->NextNode;
    		b->NextNode->PrevNode = NULL;
  		} 
    	// The block is the last block but not the first block.
		else if(b->PrevNode != NULL && b->NextNode == NULL) {
   	  		FreeBins[CandAge][CandIdx].LastBlock = b->PrevNode;
   	  		b->PrevNode->NextNode = NULL;
   		}
		// The block is between other blocks.
    	else if(b->PrevNode != NULL && b->NextNode != NULL) {
    	  	b->PrevNode->NextNode = b->NextNode;
    	  	b->NextNode->PrevNode = b->PrevNode;
  		}
 
	 	MergedBlock->Lines += b->Lines;
  		CurHeaders.erase(*iter);
	}

	MergedBlock->PrevNode = NULL;
	MergedBlock->NextNode = NULL;
	
	CurHeaders.insert(pair<UINT64, BNode>((UINT64)MergedBlock->DataAddr, MergedBlock));
	
	cout << "Merged block: 0x" << hex << (UINT64)MergedBlock << " size: " << dec << MergedBlock->Lines << " lines" << endl;
	cout << "Data address : 0x" << hex << (UINT64)MergedBlock->DataAddr << endl;
	
	return MergedBlock;
}

void FreeBlock(Bin UsedBin, Bin FreeBin, BNode b) {

	UINT32 Idx = b->Lines - 1;

	// Delete such header from used bin.
	if(b->PrevNode == NULL) {
    	// The block is the only block in used bin.
    	if(b->NextNode == NULL) {
      		UsedBin[Idx].FirstBlock = NULL;
      		UsedBin[Idx].LastBlock = NULL;
    	}
		// The block is the first block but not the last block.
    	else if(b->NextNode != NULL) {
      		UsedBin[Idx].FirstBlock = b->NextNode;
    		b->NextNode->PrevNode = NULL;
    	}  
  	}
 	else if(b->PrevNode != NULL)  {
    	// The block is the last block but not the first block.
    	if(b->NextNode == NULL) {
      		UsedBin[Idx].LastBlock = b->PrevNode;
      		b->PrevNode->NextNode = NULL;
    	}
    	// The block is between other blocks.
    	else if(b->NextNode != NULL) {
      		b->PrevNode->NextNode = b->NextNode;
      		b->NextNode->PrevNode = b->PrevNode;
    	}
  	}

	b->PrevNode = NULL;
	b->NextNode = NULL;
	b->IsFree = 1;

	// Insert such header into the tail of free bin.
	if(FreeBin[Idx].FirstBlock == NULL) {
		FreeBin[Idx].FirstBlock = b;
		FreeBin[Idx].LastBlock = b;
	}
	else {
		b->PrevNode = FreeBin[Idx].LastBlock;
		FreeBin[Idx].LastBlock->NextNode = b;
		FreeBin[Idx].LastBlock = b;
	}
}

BNode GetHeader(UINT64 DataAddr) {
	return CurHeaders[DataAddr];
}

void gfree(void *p) {
	BNode b = GetHeader((UINT64)p);
	UINT16 NewAge = CalculateAge(b->DataAddr, b->Lines);
	
	if(b->age > Generation - 1 || NewAge > Generation - 1) {
		cout << "Current age: " << b->age << endl;
		cout << "New age: " << NewAge << endl;
		cout << "The age of PCM line cannot be older than " << Generation - 1 << endl;
		exit(1);
	}
	
	//if(b->age > NewAge) {
	//	cout << "Current age: " << b->age << endl;
	//	cout << "New age: " << NewAge << endl;
	//	cout << "The age of PCM line cannot grow younger!" << endl;
	//	exit(1);
	//}
	
	cout << "Free block size: " << dec << b->Lines << " lines" << endl;
	cout << "Header address: 0x" << hex << (UINT64)b << endl;
	cout << "Data address: 0x" << hex << (UINT64)b->DataAddr << endl;
	UINT32 FreeIdx = b->Lines - 1;
	

	if(b->age == 2) {
		cout << "Free block from UsedBins[2][" << dec << FreeIdx << "] to FreeBins[2][" << dec << FreeIdx << "]" << endl;
		FreeBlock(UsedBins[2], FreeBins[2], b);
	}
	else if(b->age == NewAge) {
		cout << "Free block from UsedBins[" << b->age << "][" << dec << FreeIdx << "] to FreeBins[" << b->age << "][" << dec << FreeIdx << "]" << endl;
		FreeBlock(UsedBins[b->age], FreeBins[b->age], b);
	}
	else if(b->age < NewAge) {
		cout << "Free block from UsedBins[" << b->age << "][" << dec << FreeIdx << "] to FreeBins[" << NewAge << "][" << dec << FreeIdx << "]" << endl;
		FreeBlock(UsedBins[b->age], FreeBins[NewAge], b);
		b->age = NewAge;
	}

	cout << endl;
}
