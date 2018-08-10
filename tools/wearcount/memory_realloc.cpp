#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <map>
#include <ctime>
#include <fstream>
#include "MemOpCollector.h"
#include "basemalloc/basemalloc.h"
#include "galloc/galloc.h"
#include "nvmalloc.h"

using namespace std;

#ifndef ALLOC_NAME
#define _MALLOC(size) ffmalloc(size)
#else
#define _MALLOC(size) ALLOC_NAME(size)
#endif
#ifndef FREE_NAME
#define _FREE(p) basefree(p)
#else
#define _FREE(p) FREE_NAME(p)
#endif

int main(int argc, char **argv) {

    char *filename;
    if (argc > 1) {
        filename = argv[1];
    } else {
        filename = "/home/liwei/Workspace/Projects/wear-leveling/output/MemOpCollector/mibench/network_dijkstra.memops";
    }
    const char *output = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/memory_realloc.out";
    ofstream outfile(output);
    if (!outfile) {
        cout << "Unable to open file " << output << endl;
        exit(1);
    }

    // InitializeBins();
    nvmalloc_init(1000, 100);

    // clock_t begin_time = clock();
    // cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(filename);
    // cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;
    // map the old address to the new address
    map<UINT64, UINT64> addrmap; 
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        if ((*iter)->memOpType == MALLOC) {
            void *p = _MALLOC((*iter)->size);
            addrmap.insert(pair<UINT64, UINT64>((*iter)->memAddr, (UINT64)p));
            outfile << hex;
            outfile << "malloc\told address: 0x" << (*iter)->memAddr << ", new address: 0x" << (UINT64)p << ", size: " << (*iter)->size << endl;
        }
        if ((*iter)->memOpType == STACK_FRAME_ALLOC) {
            if ((*iter)->startMemAddr == 0)
                continue;
            void *p = _MALLOC((*iter)->size);
            addrmap.insert(pair<UINT64, UINT64>((*iter)->endMemAddr, (UINT64)p));
            outfile << hex;
            outfile << "stack_alloc\told address: 0x" << (*iter)->endMemAddr << ", new address: 0x" << (UINT64)p << ", size: " << (*iter)->size << endl;
        }
        if ((*iter)->memOpType == FREE || (*iter)->memOpType == STACK_FRAME_FREE) {
            if ((*iter)->memAddr == ULLONG_MAX)
                continue;
            map<UINT64, UINT64>::iterator it = addrmap.find((*iter)->memAddr);
            if (it == addrmap.end()) {
                outfile << "Free unallocated memory!" << endl;
                outfile << "Free address: " << (*iter)->memAddr << endl;
                exit(1);
            }
            _FREE((void*)it->second);
            addrmap.erase(it);
            outfile << hex;
            if ((*iter)->memOpType == FREE)
                outfile << "free\t";
            else 
                outfile << "stack_free\t";
            outfile << "old address: 0x" << (*iter)->memAddr << ", new address: 0x" << it->second << endl;
        }
    }

    nvmalloc_exit();
}

