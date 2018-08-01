#include <cstdlib>
#include <iostream>
#include <ctime>
#include <vector>
#include "galloc.h"

#define random(x, y) (rand() % (y - x + 1) + x)

int main()
{
	int AllocCount = 0;
	int WritesLength = 0;
	vector<char *>BlockBuffer;
   
	InitializeBins();

	srand((unsigned) time(0));

	// We should free some blocks every 'AllocTimes' times of allocation. When the first allocation 
	// series finish, the amount of remaining blocks (initialized 'RemainingBlocks') is 'AllocTimes'.
	int AllocTimes = random(1, 30);
	int RemainingBlocks = AllocTimes;

	for(int i = 0; i < 5000; i++){
		int AllocSize = random(8, 1024);
		cout << dec << i + 1 << ". Allocation size: " << dec << AllocSize << endl;
		BlockBuffer.push_back((char *)galloc(AllocSize));

		char *Block = BlockBuffer.back();
		WritesLength = random((AllocSize * 2) / 3, AllocSize);
		for(int k = 0; k < WritesLength; k++){
			int Writes = random(1, 5);
			while(Writes > 0) {
				Block[k] = random(1, 26) - 1 + 'a';
				Writes--;
			}
		}
		AllocCount++;
     
		if(AllocCount == AllocTimes){
			AllocCount = 0;
			// Randomly choose 'BlocksToFree' blocks to free. Then there are
			// still some remaining blocks to be freed. 	
			int BlocksToFree = random(1, RemainingBlocks);
			RemainingBlocks = RemainingBlocks - BlocksToFree;
	        
			while (BlocksToFree != 0) {
				int FreeIndex = random(1, BlockBuffer.size()) - 1;
				gfree(BlockBuffer[FreeIndex]);
				BlockBuffer.erase(BlockBuffer.begin() + FreeIndex);
				BlocksToFree--;
			}
	
			AllocTimes = random(1, 30);
			RemainingBlocks += AllocTimes;
		}
   }

   while(!BlockBuffer.empty()) {
       int FreeIndex = random(1, BlockBuffer.size()) - 1;
       gfree(BlockBuffer[FreeIndex]);
       BlockBuffer.erase(BlockBuffer.begin() + FreeIndex);
   }

   cout << "Finish Allocation..........." << endl;

   return 0;
}
