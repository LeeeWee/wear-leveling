#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <map>
#include <ctime>
#include <fstream>
#include "MemOpCollector.h"

// print memory operations info
void printLargeMemOperations(vector<MemOp> memoryOperations) {
    vector<MemOp>::iterator iter;
    for (iter = memoryOperations.begin(); iter != memoryOperations.end(); iter++) {
        MemOp memop = *iter;
        cout << hex;
        switch (memop->memOpType) {
            case MALLOC:
                if (memop->size < 1024)
                    break;
                cout << "malloc, address: " << hex << memop->memAddr << ", size: " << dec << memop->size << endl;
                break;
            case FREE:
                // cout << "free, address: " << hex << memop->memAddr << endl;
                break;
            case STACK_FRAME_ALLOC: {
                if (memop->size < 1024)
                    break;
                cout << "stack_frame_alloc, start_addr: " << hex << memop->startMemAddr << ", end_addr: " << memop->endMemAddr 
                    << dec << ", size: " << memop->size << ", routine_name: " << memop->rtnName << endl;
                // print stackoperations
                vector<StackOp>::iterator stackop_iter;
                for (stackop_iter = memop->stackOperations.begin(); stackop_iter != memop->stackOperations.end(); stackop_iter++) {
                    cout << "\tstack_op: ";
                    if ((*stackop_iter)->stackOpType == STACK_WRITE) cout << "STACK_WRITE, ";
                    else cout << "STACK_READ, ";
                    cout << "address: " << hex << (*stackop_iter)->stackAddr << dec << ", offset: " << (*stackop_iter)->offset << ", size: " << (*stackop_iter)->size << endl;
                }
                break;
            }
            case STACK_FRAME_FREE:
                // cout << "stack_frame_free, address: " << memop->memAddr << ", routine_name: " << memop->rtnName << endl;
                break;
        }
    }
}

int main(int argc, char **argv) {

    // InitializeBins();
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

    // clock_t begin_time = clock();
    // cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(filename);
    // cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;

    printLargeMemOperations(memOperations);

    int allocSizeArray[33] = {0};
    
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        MemOp memop = *iter;
        if (memop->memOpType == FREE || memop->memOpType == STACK_FRAME_FREE) 
            continue;
        int size = memop->size;
        int idx = size % 32 == 0 ? size / 32 : size / 32 + 1;
        if (idx > 32) idx = 32;
        allocSizeArray[idx]++;  
    }

    cout << "allocated size distribution: " << endl;
    for (int i = 0; i < 32; i++) {
        cout << 32*i << " ~ " << 32*(i+1) << " bytes: " << allocSizeArray[i] << endl;
    }
    cout << "over 1024 bytes: " << allocSizeArray[32] << endl;
}