#include "MemOpCollector.h"
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <cstring>
#include <sstream>
#include <map>
#include <climits>


using namespace std;

template <typename NUM>
NUM hexstr2num(const char *hexstr) {
    NUM i;
    stringstream ss;
    ss << hex << hexstr;
    ss >> i;
    return i;
}

// split string by space
vector<string> split(char str[]) {
    vector<string> res;
    if (str == "") return res;
    char *p = strtok(str, " ");
    while (p) {
        string s = p; // convert char * to string
        res.push_back(s);
        p = strtok(NULL, " ");
    }
    return res;
}

void processRoutines(ifstream &infile, MemOp memop, vector<MemOp> &memoryOperations) {
    static MemOp mallocMemop = new MemOperation();
    mallocMemop->memOpType = MALLOC;
    memop->startMemAddr = 0;
    memop->endMemAddr = ULLONG_MAX;
    char line[100];
    while (infile.getline(line, 100)) {
        if (strncmp("Exit", line, 4) == 0) {
            // get exit routine name
            vector<string> splits = split(line);
            string exitRtnName = splits[1];
            // if current memop name doesn't equal the exitRtnName, this means current function doesn't exit
            // iterate reversely and find the exited memop
            if (memop->rtnName != exitRtnName) {
                vector<MemOp>::reverse_iterator riter;
                for (riter = memoryOperations.rbegin(); riter != memoryOperations.rend(); riter++) {
                    if ((*riter)->rtnName == exitRtnName && (*riter)->size <= 0) {
                        break;
                    }
                } 
                if (riter == memoryOperations.rend())
                    continue;
                memop = *riter;
            }
            
            // calculate stack frame memory size
            memop->size = memop->startMemAddr - memop->endMemAddr;
            // calculate stack operations' offset to the stack frame end address
            vector<StackOp>::iterator iter;
            for (iter = memop->stackOperations.begin(); iter != memop->stackOperations.end(); iter++) {
                (*iter)->offset = (*iter)->stackAddr - memop->endMemAddr;
            }
            // free memory operation
            MemOp freeMemop = new MemOperation();
            freeMemop->memOpType = STACK_FRAME_FREE;
            freeMemop->rtnName = memop->rtnName;
            freeMemop->memAddr = memop->endMemAddr;
            memoryOperations.push_back(freeMemop);
            return;
        }
        else if (strncmp("MAIN Exited", line, 11) == 0 || strncmp("Entry _exit", line, 11) == 0 
                    || strncmp("Entry _Exit", line, 11) == 0) {
            // calculate main routine stack frame memory size
            MemOp mainMemop = memoryOperations.at(0);
            mainMemop->size = mainMemop->startMemAddr - mainMemop->endMemAddr;
            // calculate stack operations' offset to the stack frame end address
            vector<StackOp>::iterator iter;
            for (iter = mainMemop->stackOperations.begin(); iter != mainMemop->stackOperations.end(); iter++) {
                (*iter)->offset = (*iter)->stackAddr - mainMemop->endMemAddr;
            }
            MemOp freeMemop = new MemOperation();
            freeMemop->memOpType = STACK_FRAME_FREE;
            freeMemop->rtnName = mainMemop->rtnName;
            freeMemop->memAddr = mainMemop->endMemAddr;
            memoryOperations.push_back(freeMemop);
            return;
        }
        else if (strncmp("Entry", line, 5) == 0) {
            MemOp newMemop = new MemOperation();
            newMemop->memOpType = STACK_FRAME_ALLOC;
            vector<string> splits = split(line);
            newMemop->rtnName = splits[1];
            memoryOperations.push_back(newMemop);
            processRoutines(infile, newMemop, memoryOperations);
            if (newMemop->size <= 0) // omit unexites memeory operations
                return;
        }
        else if (strncmp("malloc", line, 6) == 0) {
            if (strncmp("malloc returns", line, 14) == 0) {
                vector<string> retSplits = split(line);
                mallocMemop->memAddr = hexstr2num<UINT64>(retSplits[2].c_str());
                memoryOperations.push_back(mallocMemop);
                mallocMemop = new MemOperation();
            } else {
                vector<string> splits = split(line);
                mallocMemop->size = hexstr2num<UINT16>(splits[1].c_str());
            }
        }
        else if (strncmp("free", line, 4) == 0) {
            MemOp freeMemop = new MemOperation();
            freeMemop->memOpType = FREE;
            vector<string> retSplits = split(line);
            freeMemop->memAddr = hexstr2num<UINT64>(retSplits[1].c_str());
            memoryOperations.push_back(freeMemop);
        }
        else {
            if (strncmp("write", line, 5) == 0) {
                StackOp stackop = new StackOperation();
                stackop->stackOpType = STACK_WRITE;
                // split line
                vector<string> splits = split(line);
                stackop->stackAddr = hexstr2num<UINT64>(splits[1].c_str());
                stackop->size = hexstr2num<UINT16>(splits[2].c_str());
                UINT64 startAddr = stackop->stackAddr + stackop->size;
                if (startAddr > memop->startMemAddr) memop->startMemAddr = startAddr;
                if (stackop->stackAddr < memop->endMemAddr) memop->endMemAddr = stackop->stackAddr;
                memop->stackOperations.push_back(stackop);
            } 
            else if (strncmp("read", line, 4) == 0) {
                StackOp stackop = new StackOperation();
                stackop->stackOpType = STACK_READ;
                // split line
                vector<string> splits = split(line);
                stackop->stackAddr = hexstr2num<UINT64>(splits[1].c_str());
                stackop->size = hexstr2num<UINT16>(splits[2].c_str());
                memop->stackOperations.push_back(stackop);
            }
        }
    } 
}

// remove routines unexited or with too large stack frame memory size 
void removeInvalidRoutines(vector<MemOp> &memoryOperations) {
    map<UINT64, string> memopMap;
    vector<MemOp>::iterator iter;
    for (iter = memoryOperations.begin(); iter != memoryOperations.end(); ) {
        if ((*iter)->memOpType == STACK_FRAME_ALLOC) {
            if ((*iter)->size <= 1 || (*iter)->size >= 0x10000) {
                iter = memoryOperations.erase(iter);
            }
            else if ((*iter)->startMemAddr == 0)
                iter = memoryOperations.erase(iter);
            else {
                memopMap.insert(pair<UINT64, string>((*iter)->endMemAddr, (*iter)->rtnName));
                iter++;
            }
        }
        else {
            // remove stack free operation without corresponding stack alloc operation
            if ((*iter)->memOpType == STACK_FRAME_FREE) {
                map<UINT64, string>::iterator mapIter = memopMap.find((*iter)->memAddr);
                if (mapIter != memopMap.end() && mapIter->second == (*iter)->rtnName) {
                    // if get corresponding stack alloc operation, remove the corresponding operation from memopMap
                    memopMap.erase(mapIter);
                    iter++;
                } else { // remove this memoperation
                    iter = memoryOperations.erase(iter);
                }
            }
            else 
                iter++;
        }
    }
}

// get memory operations from given file
vector<MemOp> getMemoryOperations(const char *filename) {
    vector<MemOp> memoryOperations;
    ifstream infile(filename);
    char line[100];
    if (!infile) {
        cout << "Failed to open file " << filename << endl;
        exit(1);
    }
    // skip the first line: "MAIN CALLED +++++++++++++++"
    infile.getline(line, 100);
    if (strncmp("MAIN", line, 4) != 0) {
        cout << "The input file to be parsed must started with \"MAIN CALLED +++++++++++++++\"" << endl;
        exit(1);
    }
    infile.getline(line, 100); // Entry main >>>>>>>>
    MemOp mainMemop = new MemOperation();
    mainMemop->memOpType = STACK_FRAME_ALLOC;
    mainMemop->rtnName = "main";
    memoryOperations.push_back(mainMemop);
    processRoutines(infile, mainMemop, memoryOperations);
    // remove invalid memory operations
    removeInvalidRoutines(memoryOperations);
    return memoryOperations;
}

// print memory operations info
void printMemOperations(vector<MemOp> memoryOperations) {
    vector<MemOp>::iterator iter;
    for (iter = memoryOperations.begin(); iter != memoryOperations.end(); iter++) {
        MemOp memop = *iter;
        cout << hex;
        switch (memop->memOpType) {
            case MALLOC:
                cout << "malloc, address: " << hex << memop->memAddr << ", size: " << dec << memop->size << endl;
                break;
            case FREE:
                cout << "free, address: " << hex << memop->memAddr << endl;
                break;
            case STACK_FRAME_ALLOC: {
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
                cout << "stack_frame_free, address: " << memop->memAddr << ", routine_name: " << memop->rtnName << endl;
                break;
        }
    }
}

void wearCount(vector<MemOp> memoryOperations, UINT32 stackWearCounter[], UINT32 heapWearCounter[],
                UINT64 &startAddrOfStack, UINT64 &startAddrOfHeap) {
    UINT16 line = 4; // 4 bytes, the minimal writes unit
    vector<MemOp>::iterator iter = memoryOperations.begin();
    MemOp mainMemop = *iter;
    if (mainMemop->rtnName != "main") {
        cout << "First routine should be main routine!" << endl;
        exit(1);
    }
    bool firstMalloc = true;
    startAddrOfStack = mainMemop->startMemAddr; // set stack start address 
    cout << hex;
    // cout << "stack start address: " << startAddrOfStack << endl;
    for (iter = memoryOperations.begin(); iter != memoryOperations.end(); iter++) {
        // calculate heap wear count 
        if ((*iter)->memOpType == MALLOC) {
            if (firstMalloc) { // set heap start address
                startAddrOfHeap = (*iter)->memAddr;
                firstMalloc = false;
                UINT16 size = (*iter)->size;
                UINT16 lines = (size % line == 0) ? size / line : size / line + 1;
                for (int i = 0; i < lines; i++) 
                    heapWearCounter[i] += 1;
            } else {
                UINT64 memaddr = (*iter)->memAddr;
                UINT16 size = (*iter)->size;
                UINT16 lines = (size % line == 0) ? size / line : size / line + 1;
                UINT64 memDist = memaddr - startAddrOfHeap;
                if (memDist % line != 0) {
                    cout << "The memory distance must be an intergal multiple of line(four bytes)!" << endl;
		            exit(1);
                }
                UINT32 lineNo = memDist / line;
                for (int i = 0; i < lines; i++) 
                    heapWearCounter[lineNo + i] += 1;
            }
        }
        // calculate stack wear count
        if ((*iter)->memOpType == STACK_FRAME_ALLOC) {
            cout << "Routine: " << (*iter)->rtnName << " size:" << (*iter)->size << endl;
            vector<StackOp> stackOperations = (*iter)->stackOperations;
            if (stackOperations.size() > 0) {
                vector<StackOp>::iterator stackopIter;
                for (stackopIter = stackOperations.begin(); stackopIter != stackOperations.end(); stackopIter++) {
                    if ((*stackopIter)->stackOpType == STACK_WRITE) {
                        UINT64 memaddr = (*stackopIter)->stackAddr;
                        UINT16 size = (*stackopIter)->size;
                        UINT16 lines = (size % line == 0) ? size / line : size / line + 1;
                        UINT64 memDist = startAddrOfStack - memaddr;
                        cout << "\tstack write: " << memaddr << " memDist: " << memDist << endl;
                        // if (memDist % line != 0) {
                        //     cout << "The memory distance must be an intergal multiple of line(four bytes)!" << endl;
                        //     exit(1);
                        // }
                        UINT32 lineNo = memDist / line;
                        for (int i = 0; i < lines; i++)
                            stackWearCounter[lineNo - 1 - i] += 1;
                    }
                }
            }
        }
    }
}

void printWearCount(UINT32 wearCount[], int size, UINT64 startAddr, bool isStack) {
    for (int i = 0; i < size; i++) {
        if (wearCount[i] != 0) {
            UINT64 start = isStack ? startAddr - i * 4 : startAddr + i * 4;
            UINT64 end = isStack ? startAddr - (i + 1) * 4 : startAddr + (i + 1) * 4;
            cout << i << "\t" << start << " ~ " << end << "\t" << wearCount[i] << endl;
        }
    }
}

void saveMemoryOperations(vector<MemOp> memoryOperations, const char *filename) {
    // create and open a character archive for output
    std::ofstream ofs(filename);
    // save data to archive
    {
        boost::archive::text_oarchive oa(ofs);
        // write StackOperation instance to archive
        oa << memoryOperations;
    }
}

vector<MemOp> loadMemoryOperations(const char *filename) {
    vector<MemOp> memoryOperations;
    std::ifstream ifs(filename);
    if (!ifs) {
        cout << "Unable to open file " << filename << endl;
        exit(1);
    }
    boost::archive::text_iarchive ia(ifs);
    // read state from archive
    ia >> memoryOperations;
    return memoryOperations;
}



