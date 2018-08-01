#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <map>
#include <ctime>
#include "MemOpCollector.h"
#include "basemalloc/basemalloc.h"
#include "galloc/galloc.h"
#include "nvmalloc.h"

#define MALLOC(size) ffmalloc(size)
#define FREE(p) basefree(p)

using namespace std;

int main() {
    // InitializeBins();
    const char *filename = "../../output/MemOpCollector/mibench/office_stringsearch.memops";
    clock_t begin_time = clock();
    cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(filename);
    cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;
    // map the old address to the new address
    map<UINT64, UINT64> addrmap; 
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        if ((*iter)->memOpType == MALLOC) {
            void *p = MALLOC((*iter)->size);
            addrmap.insert(pair<UINT64, UINT64>((*iter)->memAddr, (UINT64)p));
            cout << hex;
            cout << "malloc\told address: 0x" << (*iter)->memAddr << ", new address: 0x" << (UINT64)p << ", size: " << (*iter)->size << endl;
        }
        if ((*iter)->memOpType == STACK_FRAME_ALLOC) {
            if ((*iter)->startMemAddr == 0)
                continue;
            void *p = MALLOC((*iter)->size);
            addrmap.insert(pair<UINT64, UINT64>((*iter)->endMemAddr, (UINT64)p));
            cout << hex;
            cout << "stack_alloc\told address: 0x" << (*iter)->endMemAddr << ", new address: 0x" << (UINT64)p << ", size: " << (*iter)->size << endl;

        }
        if ((*iter)->memOpType == FREE || (*iter)->memOpType == STACK_FRAME_FREE) {
            if ((*iter)->memAddr == ULLONG_MAX)
                continue;
            map<UINT64, UINT64>::iterator it = addrmap.find((*iter)->memAddr);
            if (it == addrmap.end()) {
                cout << "Free unallocated memory!" << endl;
                cout << "Free address: " << (*iter)->memAddr << endl;
                exit(1);
            }
            FREE((void*)it->second);
            addrmap.erase(it);
            cout << hex;
            if ((*iter)->memOpType == FREE)
                cout << "free\t";
            else 
                cout << "stack_free\t";
            cout << "old address: 0x" << (*iter)->memAddr << ", new address: 0x" << it->second << endl;

        }
    }
}

