#include "MemOpCollector.h"
#include <stdlib.h>
#include <iostream>
#include <dirent.h>
#include <ctime>
#include <set>

using namespace std;

inline bool endwith(string const &value, string const &ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

inline set<string> listfiles(string const &dirname) {
    set<string> files;
    DIR *dir = opendir(dirname.c_str());
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        string d_name = dp->d_name;
        string filename = d_name.substr(0, d_name.length());
        files.insert(filename);
    }
    return files;
}

// collect memory operations for memtrack files under given dir, and output memop vector to binary file under output dir
void collectForDir(string dirname, string outputdir, string input_suffix=".memtrack", string output_suffix=".memops") {
    DIR *dir = opendir(dirname.c_str());
    if (!endwith(dirname, "/")) dirname += "/";
    if (!endwith(outputdir, "/")) outputdir += "/";
    set<string> outputfiles = listfiles(outputdir);
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        string d_name = dp->d_name;
        if (endwith(d_name, input_suffix)) {
            string absPath = dirname + d_name; // get absolute path
            string filename = d_name.substr(0, d_name.length()-input_suffix.length());
            // if the outputdir contains the generated file, skip current file
            set<string>::iterator iter = outputfiles.find(filename + output_suffix);
            if (iter != outputfiles.end())
                continue;
                
            string output = outputdir + filename + output_suffix; // get output path
            // get memory operations
            cout << "Collecting memory operations for " << absPath << "..." << endl;
            vector<MemOp> memOperations = getMemoryOperations(absPath.c_str());
            saveMemoryOperations(memOperations, output.c_str());
        }
    }
}


void wearcount_test() {
    const char *filename = "../PinTools/memtracker.out";
    vector<MemOp> memoryOperations = getMemoryOperations(filename);
    cout << "Num of memory operations: " << memoryOperations.size() << endl;
    printMemOperations(memoryOperations);
    UINT32 stackWearCounter[100000] = {0};
    UINT32 heapWearCounter[100000] = {0};
    UINT64 startAddrOfStack = 0;
    UINT64 startAddrOfHeap = 0;
    wearCount(memoryOperations, stackWearCounter, heapWearCounter, startAddrOfStack, startAddrOfHeap);
    cout << "stack wear count: " << endl;
    printWearCount(stackWearCounter, 100000, startAddrOfStack, true);
    cout << "heap wear count: " << endl;
    printWearCount(heapWearCounter, 100000, startAddrOfHeap, false);
}

void save_load_test() {
    // load from file
    clock_t begin_time = clock();
    string filename = "../memtracker/output/memtrack-out/mediabench2/cjpeg.memtrack";
    vector<MemOp> memOperations = getMemoryOperations(filename.c_str());
    std::cout << "Extracting memory operations cost: " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s."<< endl;

    begin_time = clock();
    saveMemoryOperations(memOperations, "./output/cjpeg.memops");
    std::cout << "Saving operations to binary file cost: " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s."<< endl;

    begin_time = clock();
    vector<MemOp> new_memOperations = loadMemoryOperations("./output/dijkstra.memops");
    std::cout << "Reading operations from binary file cost: " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s."<< endl;

    printMemOperations(new_memOperations);
}


int main(int argc, char *argv[]) {
    const char *dirname = "../memtracker/output/memtrack-out/mibench/";
    const char *outputdir = "./output/mibench";
    collectForDir(dirname, outputdir);
    return 0;
}
