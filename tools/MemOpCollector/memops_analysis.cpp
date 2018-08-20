#include <iostream>
#include <map>
#include <ctime>
#include <fstream>
#include "MemOpCollector.h"

using namespace std;

#define ALIGNMENT 8 // must be a power of 2

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

typedef pair<MemOp, int> PAIR;

struct CmpByValue {
    bool operator()(const PAIR &lhs, const PAIR &rhs) {
        return lhs.second > rhs.second;
    }
};

void getWritesFrequenceForStackFrameWrites(vector<MemOp> memOperations) {
    cout << "Getting write frequence for stack frame writes..." << endl;
    map<MemOp, int> writeFrequence;
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        if ((*iter)->memOpType != STACK_FRAME_ALLOC)
            continue;
        else {
            map<UINT64, int> writeCounts;
            // calculate write counts for every base address
            vector<StackOp>::iterator stackopIter;
            for (stackopIter = (*iter)->stackOperations.begin(); stackopIter != (*iter)->stackOperations.end(); stackopIter++) {
                StackOp stackop = *stackopIter;
                if (stackop->stackOpType == STACK_READ)
                    continue;
                else {
                    map<UINT64, int>::iterator wf_iter;
                    int newSize = ALIGN(stackop->size);
                    for (int i = 0; i < newSize / ALIGNMENT; i++) {
                        UINT64 addr = stackop->stackAddr + ALIGNMENT * i;
                        if ((wf_iter = writeCounts.find(addr)) != writeCounts.end()) 
                            wf_iter->second++;
                        else 
                            writeCounts.insert(make_pair(addr, 1));
                    }
                }
            }
            // calculate the max writes
            int max = 0;
            for (map<UINT64, int>::iterator wf_iter = writeCounts.begin(); wf_iter != writeCounts.end(); wf_iter++) {
                if (wf_iter->second > max)
                    max = wf_iter->second;
            }
            writeFrequence.insert(make_pair((*iter), max));
        }
    }
    // get max write frequence routines
    vector<PAIR> writeFrequenceVec(writeFrequence.begin(), writeFrequence.end());
    sort(writeFrequenceVec.begin(), writeFrequenceVec.end(), CmpByValue());
    for (int i = 0; i < 20; i++) {
        cout << writeFrequenceVec[i].first->rtnName << ",size: " << writeFrequenceVec[i].first->size;
        cout << ", maxWrite: " << writeFrequenceVec[i].second << endl;
    }
}

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

    // getWritesFrequenceForStackFrameWrites(memOperations);
    printMemOperations(memOperations);

}

