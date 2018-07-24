#include "MemOpCollector.h"
#include "basemalloc/basemalloc.h"

int main() {
    const char *filename = "dijkstra_memtracker.out";
    vector<MemOp> memoryOperations = getMemoryOperations(filename);
    vector<MemOp>::iterator iter;
    for (iter = memoryOperations.begin(); iter != memoryOperations.end(); iter++) {
        
    }
}