#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <map>
#include <ctime>
#include <fstream>
#include "MemOpCollector.h"

int main(int argc, char **argv) {

    char *filename;
    if (argc > 1) {
        filename = argv[1];
    } else {
        filename = "/home/liwei/Workspace/Projects/wear-leveling/output/MemOpCollector/mibench/network_dijkstra.memops";
    }

    // clock_t begin_time = clock();
    // cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(filename);
    // cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;

    // printMemOperations(memOperations);

    int allocSizeArray[34] = {0};
    
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        MemOp memop = *iter;
        if (memop->memOpType == FREE || memop->memOpType == STACK_FRAME_FREE) 
            continue;
        int size = memop->size;
        int idx;
        if (size > 4 * 1024)
            idx = 33;
        else {
            idx = size % 32 == 0 ? size / 32 : size / 32 + 1;
            if (idx > 32) idx = 32;
        }
        allocSizeArray[idx]++;  
    }

    cout << "allocated size distribution: " << endl;
    for (int i = 0; i < 32; i++) {
        cout << 32*i << " ~ " << 32*(i+1) << " bytes: " << allocSizeArray[i] << endl;
    }
    cout << "over 1024 bytes: " << allocSizeArray[32] << endl;
    cout << "over 4 kilobytes: " << allocSizeArray[33] << endl;
}