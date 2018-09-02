#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <ctime>
#include <climits>
#include <algorithm>
#include "MemOpCollector.h"

using namespace std;

// the minimal size of memory 
#define ALIGNMENT 8 
// block size for calculating wear info, must be a power of ALIGNMENT
#define BLOCK_SIZE 64

// global value
int wearcount_size;
int blocks_wearcount_size;

struct MemoryWrites {
    UINT64 address;
    UINT32 size;
};

typedef struct MemoryWrites * MemWrites;

struct MemWritesAscendingComparator {
    bool operator()(const MemWrites& lhs, const MemWrites& rhs) const {
        return lhs->address < rhs->address;
    }
};

inline bool endwith(string const &value, string const &ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

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

vector<MemWrites> getMallocWrites(vector<MemOp> &memOperations) {
    vector<MemWrites> dataWrites;
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        MemOp memop = *iter;
        if (memop->memOpType == FREE || memop->memOpType == STACK_FRAME_FREE || memop->memOpType == STACK_FRAME_ALLOC)
            continue;
        if (memop->memOpType == MALLOC) {
            MemWrites mw = new MemoryWrites();
            mw->address = memop->memAddr;
            mw->size = memop->size;
            dataWrites.push_back(mw);
        }
    }
    return dataWrites;
}

vector<MemWrites> convertToDataWrites(vector<MemOp> &memOperations, bool analyse_alloc_distribution) {
    vector<MemWrites> dataWrites;
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        MemOp memop = *iter;
        if (memop->memOpType == FREE || memop->memOpType == STACK_FRAME_FREE)
            continue;
        if (memop->memOpType == MALLOC) {
            // MemWrites mw = new MemoryWrites();
            // mw->address = memop->memAddr;
            // mw->size = memop->size;
            // dataWrites.push_back(mw);
            continue;
        }
        if (memop->memOpType == STACK_FRAME_ALLOC) {
            if (analyse_alloc_distribution) {
                bool hasWrites = false;
                vector<StackOp>::iterator stackopIter;
                for (stackopIter = memop->stackOperations.begin(); stackopIter != memop->stackOperations.end(); stackopIter++) {
                    if ((*stackopIter)->stackOpType == STACK_WRITE) {
                        hasWrites = true;
                    }
                } 
                if (hasWrites) {
                    MemWrites mw = new MemoryWrites();
                    mw->address = memop->endMemAddr;
                    mw->size = memop->size;
                    dataWrites.push_back(mw);
                }
            } else {
                vector<StackOp>::iterator stackopIter;
                for (stackopIter = memop->stackOperations.begin(); stackopIter != memop->stackOperations.end(); stackopIter++) {
                    if ((*stackopIter)->stackOpType == STACK_WRITE) {
                        MemWrites mw = new MemoryWrites();
                        mw->address = (*stackopIter)->stackAddr;
                        mw->size = (*stackopIter)->size;
                        dataWrites.push_back(mw);
                    }
                } 
            }
        }
    }
    return dataWrites;

}

// sort data writes by address
int *calculateWearcount(vector<MemWrites> dataWrites) {
    std::sort(dataWrites.begin(), dataWrites.end(), MemWritesAscendingComparator());
    vector<vector<int>> splitLineWearCounts;
    vector<MemWrites>::iterator iter;
    UINT64 startAddress = dataWrites.at(0)->address;
    UINT64 tmpAddress = startAddress;
    vector<int> tmpWearCount;
    for (iter = dataWrites.begin(); iter != dataWrites.end(); iter++) {
        MemWrites dw = (*iter);
        // when this data write's address is far from previous address, split 
        if (dw->address - tmpAddress > 0x1000) {
            splitLineWearCounts.push_back(tmpWearCount);
            tmpWearCount.clear();
            startAddress = dw->address;
        }

        // add wear count
        int startLineNo = (dw->address - startAddress) / ALIGNMENT;
        int endLineNo = (dw->address + dw->size - startAddress) % ALIGNMENT == 0 ?
            (dw->address + dw->size - startAddress) / ALIGNMENT - 1 : (dw->address + dw->size - startAddress) / ALIGNMENT;
        while (tmpWearCount.size() <= endLineNo + 1) {
            tmpWearCount.push_back(0);
        }
        for (int i = startLineNo; i <= endLineNo; i++) {
            tmpWearCount.at(i) += 1;
        }

        tmpAddress = dw->address;
    }
    splitLineWearCounts.push_back(tmpWearCount);

    // calculate blocks wear count using worst wear count
    int times = BLOCK_SIZE / ALIGNMENT;
    blocks_wearcount_size = 0;
    int sizes[splitLineWearCounts.size()] = {0};
    int index;
    for (index = 0; index < splitLineWearCounts.size(); index++) {
        vector<int> tmpWearCounts = splitLineWearCounts.at(index);
        sizes[index] = tmpWearCounts.size() / times == 0 ?
            tmpWearCounts.size() / times : tmpWearCounts.size() / times + 1;
        blocks_wearcount_size += sizes[index];
    }
    int *blocks_wearcount = new int[blocks_wearcount_size]();
    int offset = 0;
    int i, j, tmpMaxWearCount;
    for (index = 0; index < splitLineWearCounts.size(); index++) {
        vector<int> tmpWearCounts = splitLineWearCounts.at(index);
        for (i = 0; i < sizes[index]; i++) {
            tmpMaxWearCount = 0;
            for (int j = i * times; j < (i + 1) * times; j++) {
            if (j >= tmpWearCounts.size())
                break;
            if (tmpWearCounts[j] > tmpMaxWearCount) 
                tmpMaxWearCount = tmpWearCounts[j];
            }
            blocks_wearcount[offset + i] = tmpMaxWearCount;
        }
        offset += sizes[index];
    }

    return blocks_wearcount;
}

void saveWearcount2File(const char *wearcount_file, const int *data_blocks_wearcount) {
    ofstream outfile(wearcount_file);
    outfile << "data:" << endl;
    for (int i = 0; i < blocks_wearcount_size; i++) {
        outfile << i << "," << data_blocks_wearcount[i] << endl;
    }
}

void saveWearcount2File(const char *wearcount_file, const int *data_blocks_wearcount, int size) {
    ofstream outfile(wearcount_file);
    outfile << "data:" << endl;
    for (int i = 0; i < size; i++) {
        outfile << i << "," << data_blocks_wearcount[i] << endl;
    }
}

int main(int argc, char **argv) {
    bool removeLargeMemOp = false;
    if (argc <= 2) {
        cout << "Usage: ./conventional_allocation_analysis input.memops outputdir" << endl;
        return 1;
    }
    char *input = argv[1];
    string outputdir = argv[2];
    if (argc > 3) {
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-rl") == 0)
                removeLargeMemOp = true;
        }
    }

    // load memory operations
    clock_t begin_time = clock();
    cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(input);
    cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;

    if (removeLargeMemOp) {
        cout << "removing large operations..." << endl;
        removeLargeOperations(memOperations);
    }

    if (!endwith(outputdir, "/")) outputdir += "/";
    string outputfile;

    cout << "getting stack data writes..." << endl;
    vector<MemWrites> dataWrites = convertToDataWrites(memOperations, false);
    outputfile = outputdir + "stack_wearcount.out";
    cout << "calculating stack wearcount..." << endl;
    int *data_blocks_wearcount = calculateWearcount(dataWrites);
    cout << "saving wearcout info to file..." << endl;
    saveWearcount2File(outputfile.c_str(), data_blocks_wearcount);
    int *stack_writes_wearcount = data_blocks_wearcount;
    int stack_wearcount_size = blocks_wearcount_size;


    cout << "getting stack allocate distribution..." << endl;
    vector<MemWrites> frameWrites = convertToDataWrites(memOperations, true);
    outputfile = outputdir + "frame_wearcount.out";
    cout << "calculating stack frame wearcount..." << endl;
    data_blocks_wearcount = calculateWearcount(frameWrites);
    cout << "saving allocate distribution info to file..." << endl;
    saveWearcount2File(outputfile.c_str(), data_blocks_wearcount);
    int *frame_writes_wearcount = data_blocks_wearcount;
    int frame_wearcount_size = blocks_wearcount_size;

    cout << "getting stack allocate distribution..." << endl;
    vector<MemWrites> mallocWrites = getMallocWrites(memOperations);
    outputfile = outputdir + "malloc_wearcount.out";
    cout << "calculating malloc wearcount..." << endl;
    data_blocks_wearcount = calculateWearcount(mallocWrites);
    cout << "saving malloc wearcount info to file..." << endl;
    saveWearcount2File(outputfile.c_str(), data_blocks_wearcount);

    int merged_malloc_stack_wearcount_size = stack_wearcount_size + blocks_wearcount_size;
    int *merged_malloc_stack_wearcount = new int[merged_malloc_stack_wearcount_size]();
    for (int i = 0; i < merged_malloc_stack_wearcount_size; i++) {
        if (i < blocks_wearcount_size)
            merged_malloc_stack_wearcount[i] = data_blocks_wearcount[i];
        else 
            merged_malloc_stack_wearcount[i] = stack_writes_wearcount[i - blocks_wearcount_size];
    }
    outputfile = outputdir + "malloc_stack_wearcount.out";
    cout << "merge malloc and stack wearcount info to file..." << endl;
    saveWearcount2File(outputfile.c_str(), merged_malloc_stack_wearcount, merged_malloc_stack_wearcount_size);

    int merged_malloc_frame_wearcount_size = frame_wearcount_size + blocks_wearcount_size;
    int *merged_malloc_frame_wearcount = new int[merged_malloc_frame_wearcount_size]();
    for (int i = 0; i < merged_malloc_frame_wearcount_size; i++) {
        if (i < blocks_wearcount_size)
            merged_malloc_frame_wearcount[i] = data_blocks_wearcount[i];
        else 
            merged_malloc_frame_wearcount[i] = frame_writes_wearcount[i - blocks_wearcount_size];
    }
    outputfile = outputdir + "malloc_frame_wearcount.out";
    cout << "merge malloc and frame wearcount info to file..." << endl;
    saveWearcount2File(outputfile.c_str(), merged_malloc_frame_wearcount, merged_malloc_stack_wearcount_size);


    return 0;
}