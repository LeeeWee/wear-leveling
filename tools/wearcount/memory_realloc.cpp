#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <map>
#include <set>
#include <ctime>
#include <fstream>
#include "MemOpCollector.h"
#include "basemalloc/basemalloc.h"
#include "galloc/galloc.h"
#include "nvmalloc.h"
#include "walloc.h"
#include "newalloc.h"

using namespace std;

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


// remove the memory operation that allocate more than 1024 byted and the corresponding free memory
void removeLargeOperations(vector<MemOp> &memOperations) {
    std::set<UINT64> deletedAddr;
    vector<MemOp>::iterator iter = memOperations.begin();
    while (iter != memOperations.end()) {
        if ((*iter)->memOpType == MALLOC || (*iter)->memOpType == STACK_FRAME_ALLOC) {
            if ((*iter)->size > 4096) {
                if ((*iter)->memOpType == MALLOC) deletedAddr.insert((*iter)->memAddr);
                else deletedAddr.insert((*iter)->endMemAddr);
                iter = memOperations.erase(iter);
            } else {
                iter++;
            }
        } else {
            set<UINT64>::iterator addrIter;
            if ((addrIter = deletedAddr.find((*iter)->memAddr)) != deletedAddr.end()) {
                iter = memOperations.erase(iter);
                deletedAddr.erase(addrIter);
            } else {
                iter++;
            }
        }
            
    }
}

int main(int argc, char **argv) {

    char *filename;
    bool removeLargeMemOP = false;
    if (argc > 2) {
        filename = argv[1];
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-rl") == 0)
                removeLargeMemOP = true;
        }
    } else if (argc > 1) {
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

#ifdef NEWALLOC
    newalloc_init();
#else
#ifdef NVMALLOC
    nvmalloc_init(100, 100);
#else
#ifdef WALLOC
    walloc_init();
#else
    newalloc_init();
#endif
#endif
#endif

    // clock_t begin_time = clock();
    // cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(filename);

    // remove large operations
    if (removeLargeMemOP) {
        cout << "Removing large memory operations..." << endl;
        removeLargeOperations(memOperations);
    }
    
    // cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;
    // map the old address to the new address
    map<UINT64, UINT64> addrmap; 
    vector<MemOp>::iterator iter;
    int count = 0;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        count++;
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
            if ((*iter)->memAddr == 0)
                continue;

            if ((*iter)->rtnName == "main")
                cout << "main" << endl;

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

#ifdef NEWALLOC
    newalloc_exit();
#else
#ifdef  NVMALLOC
    nvmalloc_exit();
#else
#ifdef WALLOC
    walloc_exit();
#else
    newalloc_exit();
#endif
#endif
#endif

}

