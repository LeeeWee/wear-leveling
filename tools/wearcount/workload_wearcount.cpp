#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sstream>
#include <map>
#include <set>
#include <ctime>
#include <climits>
#include <algorithm>

using namespace std;


// block size for calculating distribution info
#define BLOCK_SIZE 64

// global value
int wearcount_size;
int blocks_wearcount_size;

struct MemoryWrites {
    uint64_t address;
    uint32_t size;
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

vector<MemWrites> loadMemoryWrites(const char *workload_file, uint64_t &startAddress, uint64_t &endAddress) {
    vector<MemWrites> memoryWrites;
    uint64_t startAddressOfDataWrites = ULLONG_MAX;
    uint64_t endAddressOfDataWrites = 0;
    ifstream infile(workload_file);
    if (!infile) {
        cout << "Unable to open file " << workload_file << endl;
        exit(1);
    }
    string line;
    while (getline(infile,line)) {
        string opType = line.substr(0, line.find(" "));
        if (opType == "alloc") {
            vector<string> splits = split(line.substr(line.find(" ")+1), " ");
            string addrStr = splits.at(1);
            uint64_t addr = hexstr2num<uint64_t>(addrStr.substr(5).c_str()); // get address from string "new address: ..."
            string sizeStr = splits.at(2);
            uint32_t size = hexstr2num<uint32_t>(sizeStr.substr(5).c_str());
            MemWrites memWrites = new MemoryWrites();
            memWrites->address = addr;
            memWrites->size = size;
            memoryWrites.push_back(memWrites);
            if (addr < startAddressOfDataWrites) 
                 startAddressOfDataWrites = addr;
            if (addr + size > endAddressOfDataWrites)
                endAddressOfDataWrites = addr + size;
        }
    }
    startAddress = startAddressOfDataWrites;
    endAddress = endAddressOfDataWrites;
    return memoryWrites;
}


int *calculateWearCount(vector<MemWrites> memoryWrites, uint64_t startAddress, uint64_t endAddress) {
    blocks_wearcount_size = (endAddress - startAddress) / BLOCK_SIZE;
    // first count distribution by the size of ALOGNMENT
    int *wearcount = new int[blocks_wearcount_size]();
    vector<MemWrites>::iterator iter;
    for (iter = memoryWrites.begin(); iter != memoryWrites.end(); iter++) {
        int startLineNo = ((*iter)->address - startAddress) / BLOCK_SIZE;
        int endLineNo = ((*iter)->address + (*iter)->size - startAddress) % BLOCK_SIZE == 0 ?
            ((*iter)->address + (*iter)->size - startAddress) / BLOCK_SIZE - 1 : ((*iter)->address + (*iter)->size - startAddress) / BLOCK_SIZE;
        for (int i = startLineNo; i <= endLineNo; i++) {
            wearcount[i] += 1;
        }
    }
    return wearcount;
}

int *calculateWearCountForMalloc(vector<MemWrites> memoryWrites) {
    std::sort(memoryWrites.begin(), memoryWrites.end(), MemWritesAscendingComparator());
    vector<vector<int>> splitWearCount;
    vector<MemWrites>::iterator iter;
    uint64_t startAddress = memoryWrites.at(0)->address;
    uint64_t tmpAddress = startAddress;
    vector<int> tmpWearCount;
    for (iter = memoryWrites.begin(); iter != memoryWrites.end(); iter++) {
        MemWrites mw = (*iter);
        // when this data write's address is far from previous address, split 
        if (mw->address - tmpAddress > 0x1000) {
            splitWearCount.push_back(tmpWearCount);
            tmpWearCount.clear();
            startAddress = mw->address;
        }

        int startLineNo = (mw->address - startAddress) / BLOCK_SIZE;
        int endLineNo = (mw->address + mw->size - startAddress) % BLOCK_SIZE == 0 ?
            (mw->address + mw->size - startAddress) / BLOCK_SIZE - 1 : (mw->address + mw->size - startAddress) / BLOCK_SIZE;
        while (tmpWearCount.size() <= endLineNo + 1) {
            tmpWearCount.push_back(0);
        }
        for (int i = startLineNo; i <= endLineNo; i++) {
            tmpWearCount.at(i) += 1;
        }

        tmpAddress = mw->address;
    }
    splitWearCount.push_back(tmpWearCount);

    // calculate blocks wear count using worst wear count
    blocks_wearcount_size = 0;
    int sizes[splitWearCount.size()] = {0};
    int index;
    for (index = 0; index < splitWearCount.size(); index++) {
        vector<int> tmpWearCount = splitWearCount.at(index);
        sizes[index] = tmpWearCount.size();
        blocks_wearcount_size += sizes[index];
    }
    int *wearcount = new int[blocks_wearcount_size]();
    int offset = 0;
    int i, j;
    for (index = 0; index < splitWearCount.size(); index++) {
        vector<int> tmpWearCount = splitWearCount.at(index);
        for (i = 0; i < sizes[index]; i++) {
            wearcount[offset + i] = tmpWearCount.at(i);
        }
        offset += sizes[index];
    }

    return wearcount;
}

void saveWearCount2File(const char *workload_wearcount_file, const int *wearcount) {
    ofstream outfile(workload_wearcount_file);
    outfile << "data:" << endl;
    for (int i = 0; i < blocks_wearcount_size; i++) {
        outfile << i << "," << wearcount[i] << endl;
    }
}

int main(int argc, char **argv) {
    bool forMalloc = false;
    if (argc >= 2) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-malloc") == 0)
                forMalloc = true;
        }
    }
    const char *workload_file = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/workload.out";
    const char *workload_wearcount_file = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/workload_wearcount.out";

    uint64_t startAddress, endAddress;
    vector<MemWrites> allocateRecords = loadMemoryWrites(workload_file, startAddress, endAddress);
    int *wearcount;
    if (forMalloc)
        wearcount = calculateWearCountForMalloc(allocateRecords);
    else
        wearcount = calculateWearCount(allocateRecords, startAddress, endAddress);
    saveWearCount2File(workload_wearcount_file, wearcount);

    return 0;
}


