#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <ctime>
#include <climits>
#include <algorithm>
#include "MemOpCollector.h"
#include "basemalloc/basemalloc.h"
#include "galloc/galloc.h"
#include "nvmalloc.h"

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

// convert string to the specific type number
template <typename NUM>
NUM hexstr2num(const char *hexstr) {
    NUM i;
    stringstream ss;
    ss << hex << hexstr;
    ss >> i;
    return i;
}

// split string by space
vector<string> split(const string& s, const std::string& c)
{
    vector<string> v;
    string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(string::npos != pos2)
    {
        v.push_back(s.substr(pos1, pos2-pos1));

        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if(pos1 != s.length())
        v.push_back(s.substr(pos1));
    return v;
}

// load metadata write records from metadata writes file
vector<MemWrites> loadMetadataWrites(const char *metadata_writes_file) {
    vector<MemWrites> metadataWrites;
    ifstream infile(metadata_writes_file);
    if (!infile) {
        cout << "Unable to open file " << metadata_writes_file << endl;
        exit(1);
    }
    string line;
    string beginStr("metadata address: ");
    int beginLength = beginStr.length();
    while (getline(infile, line)) {
        size_t commaPos = line.find(",");
        UINT64 address = hexstr2num<UINT64>(line.substr(beginLength, commaPos-beginLength).c_str());
        UINT32 size = hexstr2num<UINT32>(line.substr(commaPos + 8).c_str());
        MemWrites mw = new MemoryWrites();
        mw->address = address;
        mw->size = size;
        metadataWrites.push_back(mw);
    }
    return metadataWrites;
}

// convert old memory operations to new memory operation with reallocated address
vector<MemOp> mapMemOps2NewAddress(vector<MemOp> &memOperations, const char *memory_realloc_file) {
    vector<MemOp> newMemOperations;
    ifstream infile(memory_realloc_file);
    if (!infile) {
        cout << "Unable to open file " << memory_realloc_file << endl;
        exit(1);
    }
    string line;
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        MemOp memop = *iter;
        if (memop->memOpType == STACK_FRAME_ALLOC && memop->startMemAddr == 0) 
            continue;
        if (memop->memOpType == STACK_FRAME_FREE && memop->memAddr == ULLONG_MAX)
            continue;
        // read line
        getline(infile, line);
        string opType = line.substr(0, line.find("\t"));
        vector<string> splits = split(line.substr(line.find("\t")+1), ",");
        string oldAddrStr = splits.at(0);
        UINT64 oldAddr = hexstr2num<UINT64>(oldAddrStr.substr(13).c_str()); // get address from string "old address: ..."
        string newAddrStr = splits.at(1);
        UINT64 newAddr = hexstr2num<UINT64>(newAddrStr.substr(13).c_str()); // get address from string "new address: ..."
        // check whether the memory oprtation and the line is matched
        if ((memop->memOpType == MALLOC && opType != "malloc") || (memop->memOpType == STACK_FRAME_ALLOC && opType != "stack_alloc") ||
            (memop->memOpType == FREE && opType != "free") || (memop->memOpType == STACK_FRAME_FREE && opType != "stack_free")) {
            cout << "Mismatch Error!" << endl; 
            exit(1);
        }
        // if the memory operation address is mismatch
        if (memop->memOpType == STACK_FRAME_ALLOC) {
            if (memop->endMemAddr != oldAddr) {
                cout << "Mismatch Error!" << endl; 
                exit(1);
            }
        } else {
            if (memop->memAddr != oldAddr) {
                cout << "Mismatch Error!" << endl; 
                exit(1);
            }
        }

        // create an instance of new MemOp
        MemOp newMemop = new MemOperation();
        newMemop->memOpType = memop->memOpType;
        if (memop->memOpType == MALLOC) {
            newMemop->memAddr = newAddr;
            newMemop->size = memop->size;
        } else if (memop->memOpType == FREE) {
            newMemop->memAddr = newAddr;
        } else if (memop->memOpType == STACK_FRAME_FREE) {
            newMemop->memAddr = newAddr;
            newMemop->rtnName = memop->rtnName;
        } else {
            newMemop->endMemAddr = newAddr;
            newMemop->startMemAddr = newAddr + memop->size;
            newMemop->size = memop->size;
            newMemop->rtnName = memop->rtnName;
            vector<StackOp>::iterator stackopIter;
            for (stackopIter = memop->stackOperations.begin(); stackopIter != memop->stackOperations.end(); stackopIter++) {
                StackOp newStackop = new StackOperation();
                newStackop->stackOpType = (*stackopIter)->stackOpType;
                newStackop->size = (*stackopIter)->size;
                newStackop->offset = (*stackopIter)->offset;
                newStackop->stackAddr = (*stackopIter)->offset + newAddr;
                // push back new stack operation
                newMemop->stackOperations.push_back(newStackop);
            }
        }
        // push back new memory operation
        newMemOperations.push_back(newMemop);
    }

    return newMemOperations;
}

// get data writes from memoperations
vector<MemWrites> getDataWrites(vector<MemOp> memOperations) {
    vector<MemWrites> dataWrites;
    vector<MemOp>::iterator iter;
    for (iter = memOperations.begin(); iter != memOperations.end(); iter++) {
        MemOp memop = *iter;
        if (memop->memOpType == FREE || memop->memOpType == STACK_FRAME_FREE)
            continue;
        if (memop->memOpType == MALLOC) {
            MemWrites mw = new MemoryWrites();
            mw->address = memop->memAddr;
            mw->size = memop->size;
            dataWrites.push_back(mw);
        }
        if (memop->memOpType == STACK_FRAME_ALLOC) {
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
    return dataWrites;
}

// given the datawrites and metadatawrites, get the start address and end address
void getStartAndEndAddress(vector<MemWrites> dataWrites, vector<MemWrites> metadataWrites, UINT64 &startAddress, UINT64 &endAddress) {
    UINT64 startAddressOfDataWrites = ULLONG_MAX;
    UINT64 endAddressOfDataWrites = 0;
    vector<MemWrites>::iterator iter;
    for (iter = dataWrites.begin(); iter != dataWrites.end(); iter++) {
        if ((*iter)->address < startAddressOfDataWrites)
            startAddressOfDataWrites = (*iter)->address;
        if ((*iter)->address + (*iter)->size > endAddressOfDataWrites)
            endAddressOfDataWrites = (*iter)->address + (*iter)->size;
    }
    UINT64 startAddressOfMetadataWrites = ULLONG_MAX;
    UINT64 endAddressOfMetadataWrites = 0;
    for (iter = metadataWrites.begin(); iter != metadataWrites.end(); iter++) {
        if ((*iter)->address < startAddressOfMetadataWrites)
            startAddressOfMetadataWrites = (*iter)->address;
        if ((*iter)->address + (*iter)->size > endAddressOfMetadataWrites)
            endAddressOfMetadataWrites = (*iter)->address + (*iter)->size;
    }
    cout << hex;
    // cout << "start address of data writes: " << startAddressOfDataWrites << endl;
    // cout << "end address of data writes: " << endAddressOfDataWrites << endl;
    // cout << "start address of metadata writes: " << startAddressOfMetadataWrites << endl;
    // cout << "end address of metadata writes: " << endAddressOfMetadataWrites << endl;
    
    // get the value of startAddress and endAddress
    startAddress = min(startAddressOfDataWrites, startAddressOfMetadataWrites);
    endAddress = max(endAddressOfDataWrites, endAddressOfMetadataWrites);
}

int *calculateWearcount(vector<MemWrites> memwrites, UINT64 startAddress, UINT64 endAddress) {
    wearcount_size = (endAddress - startAddress) / ALIGNMENT;
    blocks_wearcount_size = (endAddress - startAddress) % BLOCK_SIZE  == 0 ? 
        (endAddress - startAddress) / BLOCK_SIZE : (endAddress - startAddress) / BLOCK_SIZE + 1;
    // first count wear by the size of ALOGNMENT
    int *wearcount = new int[wearcount_size]();
    vector<MemWrites>::iterator iter;
    for (iter = memwrites.begin(); iter != memwrites.end(); iter++) {
        int startLineNo = ((*iter)->address - startAddress) / ALIGNMENT;
        int endLineNo = ((*iter)->address + (*iter)->size - startAddress) % ALIGNMENT == 0 ?
            ((*iter)->address + (*iter)->size - startAddress) / ALIGNMENT - 1 : ((*iter)->address + (*iter)->size - startAddress) / ALIGNMENT;
        for (int i = startLineNo; i <= endLineNo; i++) {
            wearcount[i] += 1;
        }
    }

    // calculate the sum of wear by the size of BLOCK_SIZE
    int times = BLOCK_SIZE / ALIGNMENT;
    int *blocks_wearcount = new int[blocks_wearcount_size]();
    for (int j = 0; j < blocks_wearcount_size; j++) {
        for (int k = j * times; k < (j + 1) * times; k++) {
            if (k >= wearcount_size)
                break;
            if (wearcount[k] > blocks_wearcount[j]) 
                blocks_wearcount[j] = wearcount[k];
        }
    }
    return blocks_wearcount;
}

void saveWearcount2File(const char *wearcount_file, const int *metadata_blocks_wearcount, const int *data_blocks_wearcount) {
    ofstream outfile(wearcount_file);
    outfile << "metadata:" << endl;
    for (int i = 0; i < blocks_wearcount_size; i++) {
        outfile << i << "," << metadata_blocks_wearcount[i] << endl;
    }
    outfile << "\ndata:" << endl;
    for (int i = 0; i < blocks_wearcount_size; i++) {
        outfile << i << "," << data_blocks_wearcount[i] << endl;
    }
}

int main() {
    const char *memory_realloc_file = "../../output/wearcount/memory_realloc.out";
    const char *metadata_writes_file = "../../output/wearcount/metadata_writes.out";
    const char *memops_file = "../../output/MemOpCollector/mibench/network_dijkstra.memops";
    const char *wearcount_file = "../../output/wearcount/wearcount.out";

    // load memory operations
    clock_t begin_time = clock();
    cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(memops_file);
    cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;

    // map the old address to the new address
    cout << "map memory operations old address to new address..." << endl;
    vector<MemOp> newMemOperations = mapMemOps2NewAddress(memOperations, memory_realloc_file);

    // dealloc memOPerations vector
    vector<MemOp>().swap(memOperations);
    
    // get data writes
    cout << "getting data writes..." << endl;
    vector<MemWrites> dataWrites = getDataWrites(newMemOperations);
    
    // get metadata writes
    cout << "getting metadata writes..." << endl;
    vector<MemWrites> metadataWrites = loadMetadataWrites(metadata_writes_file);

    // get the start address and end address
    UINT64 startAddress, endAddress;
    getStartAndEndAddress(dataWrites, metadataWrites, startAddress, endAddress);
    
    cout << "calculating wearcount..." << endl;
    int *metadata_blocks_wearcount = calculateWearcount(metadataWrites, startAddress, endAddress);
    int *data_blocks_wearcount = calculateWearcount(dataWrites, startAddress, endAddress);

    cout << "saving wearcout info to file..." << endl;
    saveWearcount2File(wearcount_file, metadata_blocks_wearcount, data_blocks_wearcount);
}