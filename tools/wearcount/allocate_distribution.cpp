#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <string>
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
int distribution_size;
int blocks_distribution_size;

struct AllocateRecord {
    uint64_t address;
    uint32_t size;
};

typedef struct AllocateRecord * AllocRecord;

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

vector<AllocRecord> loadAllocateRecords(const char *memory_realloc_file, uint64_t &startAddress, uint64_t &endAddress) {
    vector<AllocRecord> allocateRecords;
    uint64_t startAddressOfDataWrites = ULLONG_MAX;
    uint64_t endAddressOfDataWrites = 0;
    ifstream infile(memory_realloc_file);
    if (!infile) {
        cout << "Unable to open file " << memory_realloc_file << endl;
        exit(1);
    }
    string line;
    while (getline(infile,line)) {
        string opType = line.substr(0, line.find("\t"));
        if (opType == "stack_alloc" || opType == "malloc") {
            vector<string> splits = split(line.substr(line.find("\t")+1), ",");
            string oldAddrStr = splits.at(0);
            uint64_t oldAddr = hexstr2num<uint64_t>(oldAddrStr.substr(13).c_str()); // get address from string "old address: ..."
            string newAddrStr = splits.at(1);
            uint64_t newAddr = hexstr2num<uint64_t>(newAddrStr.substr(13).c_str()); // get address from string "new address: ..."
            string sizeStr = splits.at(2);
            uint32_t size = hexstr2num<uint32_t>(sizeStr.substr(6).c_str());
            AllocRecord allocRec = new AllocateRecord();
            allocRec->address = newAddr;
            allocRec->size = size;
            allocateRecords.push_back(allocRec);
            if (newAddr < startAddressOfDataWrites) 
                 startAddressOfDataWrites = newAddr;
            if (newAddr + size > endAddressOfDataWrites)
                endAddressOfDataWrites = newAddr + size;
        }
    }
    startAddress = startAddressOfDataWrites;
    endAddress = endAddressOfDataWrites;
    return allocateRecords;
}


int *calculateAllocateDistribution(vector<AllocRecord> allocateRecords, uint64_t startAddress, uint64_t endAddress) {
    distribution_size = (endAddress - startAddress) / BLOCK_SIZE;
    // first count distribution by the size of ALOGNMENT
    int *distribution = new int[distribution_size]();
    vector<AllocRecord>::iterator iter;
    for (iter = allocateRecords.begin(); iter != allocateRecords.end(); iter++) {
        int startLineNo = ((*iter)->address - startAddress) / BLOCK_SIZE;
        int endLineNo = ((*iter)->address + (*iter)->size - startAddress) % BLOCK_SIZE == 0 ?
            ((*iter)->address + (*iter)->size - startAddress) / BLOCK_SIZE - 1 : ((*iter)->address + (*iter)->size - startAddress) / BLOCK_SIZE;
        for (int i = startLineNo; i <= endLineNo; i++) {
            distribution[i] += 1;
        }
    }
    return distribution;
}

void saveAllocDistribution2File(const char *alloc_distribution_file, const int *distribution) {
    ofstream outfile(alloc_distribution_file);
    outfile << "distribution:" << endl;
    for (int i = 0; i < distribution_size; i++) {
        outfile << i << "," << distribution[i] << endl;
    }
}

int main() {
    const char *memory_realloc_file = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/memory_realloc.out";
    const char *alloc_distribution_file = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/alloc_distribution.out";

    uint64_t startAddress, endAddress;
    vector<AllocRecord> allocateRecords = loadAllocateRecords(memory_realloc_file, startAddress, endAddress);
    int *distribution = calculateAllocateDistribution(allocateRecords, startAddress, endAddress);
    saveAllocDistribution2File(alloc_distribution_file, distribution);

    return 0;
}


