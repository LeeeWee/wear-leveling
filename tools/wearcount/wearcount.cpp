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

// get the start address and end address only considering the datawrites
void getStartAndEndAddress(vector<MemWrites> dataWrites, UINT64 &startAddress, UINT64 &endAddress) {
    UINT64 startAddressOfDataWrites = ULLONG_MAX;
    UINT64 endAddressOfDataWrites = 0;
    vector<MemWrites>::iterator iter;
    for (iter = dataWrites.begin(); iter != dataWrites.end(); iter++) {
        if ((*iter)->address < startAddressOfDataWrites)
            startAddressOfDataWrites = (*iter)->address;
        if ((*iter)->address + (*iter)->size > endAddressOfDataWrites)
            endAddressOfDataWrites = (*iter)->address + (*iter)->size;
    }
    startAddress = startAddressOfDataWrites;
    endAddress = endAddressOfDataWrites;
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

    // calculate the wear count by the size of BLOCK_SIZE
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


// sort data writes by address
int *calculateMallocWearcount(vector<MemWrites> dataWrites) {
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
    int *blocks_wearcount = new int[blocks_wearcount_size];
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

void saveWearcount2File(const char *wearcount_file, const int *data_blocks_wearcount) {
    ofstream outfile(wearcount_file);
    outfile << "data:" << endl;
    for (int i = 0; i < blocks_wearcount_size; i++) {
        outfile << i << "," << data_blocks_wearcount[i] << endl;
    }
}

// remove the memory operation that allocate more than 1024 byted and the corresponding free memory
void removeLargeOperations(vector<MemOp> &memOperations) {
    std::set<UINT64> deletedAddr;
    vector<MemOp>::iterator iter = memOperations.begin();
    while (iter != memOperations.end()) {
        if ((*iter)->memOpType == MALLOC || (*iter)->memOpType == STACK_FRAME_ALLOC) {
            if ((*iter)->size > 1024) {
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
    char *memops_file;
    bool consideringMetadata = false;
    bool removeLargeMemOp = false;
    bool forMalloc = false;
    if (argc > 2) {
        memops_file = argv[1];
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-cm") == 0) 
                consideringMetadata = true;
            if (strcmp(argv[i], "-rl") == 0)
                removeLargeMemOp = true;
            if (strcmp(argv[i], "-malloc") == 0)
                forMalloc = true;
        }
    } else if (argc > 1) {
        memops_file = argv[1];
    }else {
        memops_file = "/home/liwei/Workspace/Projects/wear-leveling/output/MemOpCollector/mibench/network_dijkstra.memops";
    }
    const char *memory_realloc_file = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/memory_realloc.out";
    const char *metadata_writes_file = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/metadata_writes.out";
    const char *wearcount_file = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/wearcount.out";

    // load memory operations
    clock_t begin_time = clock();
    cout << "Loading memory operations..." << endl;
    vector<MemOp> memOperations = loadMemoryOperations(memops_file);
    cout << "Finished loading, cost " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s" << endl;
    
    if (removeLargeMemOp) {
        cout << "removing large operations..." << endl;
        removeLargeOperations(memOperations);
    }

    // map the old address to the new address
    cout << "map memory operations old address to new address..." << endl;
    vector<MemOp> newMemOperations = mapMemOps2NewAddress(memOperations, memory_realloc_file);

    // dealloc memOperations vector
    vector<MemOp>().swap(memOperations);
    
    // get data writes
    cout << "getting data writes..." << endl;
    vector<MemWrites> dataWrites = getDataWrites(newMemOperations);
    
    if (forMalloc) {
        cout << "calculating wearcount..." << endl;
        int *data_blocks_wearcount = calculateMallocWearcount(dataWrites);
        cout << "saving wearcout info to file..." << endl;
        saveWearcount2File(wearcount_file, data_blocks_wearcount);
        return 0;
    }

    UINT64 startAddress, endAddress;

    if (consideringMetadata) {
        // get metadata writes
        cout << "getting metadata writes..." << endl;
        vector<MemWrites> metadataWrites = loadMetadataWrites(metadata_writes_file);
        // get the start address and end address
        getStartAndEndAddress(dataWrites, metadataWrites, startAddress, endAddress);
        cout << hex << "startAddress: " << startAddress << ", endAddress: " << endAddress << endl;

        cout << "calculating wearcount..." << endl;
        int *metadata_blocks_wearcount = calculateWearcount(metadataWrites, startAddress, endAddress);
        int *data_blocks_wearcount = calculateWearcount(dataWrites, startAddress, endAddress);

        cout << "saving wearcout info to file..." << endl;
        saveWearcount2File(wearcount_file, metadata_blocks_wearcount, data_blocks_wearcount);
    }
    else {

        // get the start address and end address
        getStartAndEndAddress(dataWrites, startAddress, endAddress);
        cout << hex << "startAddress: " << startAddress << ", endAddress: " << endAddress << endl;

        cout << "calculating wearcount..." << endl;
        int *data_blocks_wearcount = calculateWearcount(dataWrites, startAddress, endAddress);

        cout << "saving wearcout info to file..." << endl;
        saveWearcount2File(wearcount_file, data_blocks_wearcount);
    }
}