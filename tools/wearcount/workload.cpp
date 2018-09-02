#include <cstdlib>
#include <iostream>
#include <fstream>
#include <ctime>
#include <vector>
#include "basemalloc/basemalloc.h"
#include "nvmalloc.h"
#include "walloc.h"
#include "newalloc.h"

using namespace std;

#define ALLOC_RANGE_LOW 10
#define ALLOC_RANGE_HIGH 4096
#define ALLOC_TIMES 500000

#define random(x, y) (rand() % (y - x + 1) + x)

#ifdef NEWALLOC
#define _MALLOC(size) newalloc(size)
#define _FREE(p) newfree(p)
#else
#ifdef NVMALLOC
#define _MALLOC(size) nvmalloc(size)
#define _FREE(p) nvfree(p)
#else
#ifdef WALLOC
#define _MALLOC(size) walloc(size)
#define _FREE(p) wfree(p)
#else 
#ifdef GLIBC_MALLOC
#define _MALLOC(size) malloc(size)
#define _FREE(p) free(p)
#else 
#ifdef FFMALLOC
#define _MALLOC(size) ffmalloc(size)
#define _FREE(p) basefree(p)
#else 
#ifdef NFMALLOC
#define _MALLOC(size) nfmalloc(size)
#define _FREE(p) basefree(p)
#else 
#ifdef BFMALLOC
#define _MALLOC(size) bfmalloc(size)
#define _FREE(p) basefree(p)
#else 
#define _MALLOC(size) newalloc(size)
#define _FREE(p) newfree(p)
#endif
#endif
#endif
#endif
#endif
#endif
#endif


int main()
{
    const char *output = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/workload.out";
    ofstream outfile(output);
    if (!outfile) {
        cout << "Unable to open file " << output << endl;
        exit(1);
    }

#ifdef NEWALLOC
    newalloc_init();
#else
#ifdef NVMALLOC
    nvmalloc_init(100, 100000000);
#else
#ifdef WALLOC
    walloc_init();
#else
#ifdef GLIBC_MALLOC
#else
    newalloc_init();
#endif
#endif
#endif
#endif

    int totalAllocTimes = 0;
	int AllocCount = 0;
	std::vector<char *>BlockBuffer;
   
	srand((unsigned) 10);

	// We should free some blocks every 'AllocTimes' times of allocation. When the first allocation 
	// series finish, the amount of remaining blocks (initialized 'RemainingBlocks') is 'AllocTimes'.
	int AllocTimes = random(1, 10);
	int RemainingBlocks = AllocTimes;

	while (totalAllocTimes < ALLOC_TIMES){
		int AllocSize = random(ALLOC_RANGE_LOW, ALLOC_RANGE_HIGH);
		BlockBuffer.push_back((char *)_MALLOC(AllocSize));

		char *Block = BlockBuffer.back();
		for(int k = 0; k < AllocSize; k++){
            Block[k] = random(1, 26) - 1 + 'a';	
		}
		AllocCount++;
        totalAllocTimes++;

        outfile << "alloc " << dec << totalAllocTimes << hex << " addr:" << (void *)Block << " size:0x" << hex << AllocSize << endl;

		if(AllocCount == AllocTimes){
			AllocCount = 0;
			// Randomly choose 'BlocksToFree' blocks to free. Then there are
			// still some remaining blocks to be freed. 	
			int BlocksToFree = random(1, RemainingBlocks);
			RemainingBlocks = RemainingBlocks - BlocksToFree;
	        
			while (BlocksToFree != 0) {
				int FreeIndex = random(1, BlockBuffer.size()) - 1;
				_FREE(BlockBuffer[FreeIndex]);

                outfile << "free " << hex << (void *)BlockBuffer[FreeIndex] << endl;

				BlockBuffer.erase(BlockBuffer.begin() + FreeIndex);
				BlocksToFree--;
			}
	
			AllocTimes = random(1, 30);
			RemainingBlocks += AllocTimes;
		}
   }

   while(!BlockBuffer.empty()) {
       int FreeIndex = random(1, BlockBuffer.size()) - 1;
       _FREE(BlockBuffer[FreeIndex]);

       outfile << "free " << hex << (void *)BlockBuffer[FreeIndex] << endl;

       BlockBuffer.erase(BlockBuffer.begin() + FreeIndex);
   }

   cout << "Finish Allocation..........." << endl;

#ifdef NEWALLOC
    newalloc_exit();
#else
#ifdef  NVMALLOC
    nvmalloc_exit();
#else
#ifdef WALLOC
    walloc_exit();
#ifdef GLIBC_MALLOC
#else
    newalloc_exit();
#endif
#endif
#endif
#endif

   return 0;
}
