#include <fstream>
#include <iostream>
// include headers that implement a archive in simple text format
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>


#include "../MemOpCollector/MemOpCollector.h"

using namespace std;

/**
namespace boost {
    namespace serialization {

        template<typename Archive>
        void serialize(Archive &ar, StackOperation &stackop, const unsigned int version) {
            ar & stackop.stackOpType;
            ar & stackop.stackAddr;
            ar & stackop.offset;
            ar & stackop.size;
        }

        template<typename Archive> 
        void serialize(Archive &ar, MemOperation &memop, const unsigned int version) {
            ar & memop.memOpType;
            ar & memop.memAddr;
            ar & memop.startMemAddr;
            ar & memop.endMemAddr;
            ar & memop.size;
            ar & memop.stackOperations;
            ar & memop.rtnName;
        }

    }
}
*/

void printStackOp(StackOp stackop) {
    cout << "stackop:(type: ";
    if (stackop->stackOpType == STACK_WRITE) 
        cout << "STACK_WRITE";
    else 
        cout << "STACK_READ";
    cout << ", address: " << std::hex << stackop->stackAddr;
    cout << ", offset: " << stackop->offset;
    cout << ", size: " << std::dec << stackop->size << ")" << std::endl;
}


void printStackOpVector(vector<StackOp> stackops) {
    vector<StackOp>::iterator iter;
    for (iter = stackops.begin(); iter != stackops.end(); iter++) {
        cout << "\t";
        printStackOp(*iter);
    }
}

void printMemOp(MemOp memop) {
    if (memop->memOpType == MALLOC) {
        cout << "memop(type: MALLOC, address: " << hex << memop->memAddr <<
            ", size: " << dec << memop->size << ")" << endl;
    } else if (memop->memOpType == FREE) {
        cout << "memop(type: FREE, address: " << hex << memop->memAddr << ")" << endl;
    } else if (memop->memOpType == STACK_FRAME_FREE) {
        cout << "memop(type: STACK_FRAME_FREE, address: " << hex << memop->memAddr << 
            ", rtnName: " << memop->rtnName << ")" << endl;
    } else if (memop->memOpType == STACK_FRAME_ALLOC) {
        cout << "memop(type: MALLOC, start_address: " << hex << memop->startMemAddr << 
            ", end_address: " << memop->endMemAddr << ", rtnName: " << memop->rtnName << ")" << endl;
        printStackOpVector(memop->stackOperations);
    }
}

void printMemOpVector(vector<MemOp> memops) {
    vector<MemOp>::iterator iter;
    for (iter = memops.begin(); iter != memops.end(); iter++) {
        printMemOp(*iter);
    }
}


vector<StackOp> createStackOpVector() {
    vector<StackOp> stackops;
    StackOp stackop0 = new StackOperation();
    stackop0->stackOpType = STACK_WRITE;
    stackop0->stackAddr = 0x456;
    stackop0->offset = 0x123;
    stackop0->size = 16;
    stackops.push_back(stackop0);

    StackOp stackop1 = new StackOperation();
    stackop1->stackOpType = STACK_READ;
    stackop1->stackAddr = 0x234;
    stackop1->offset = 0x100;
    stackop1->size = 16;
    stackops.push_back(stackop1);

    StackOp stackop2 = new StackOperation();
    stackop2->stackOpType = STACK_WRITE;
    stackop2->stackAddr = 0x158;
    stackop2->offset = 0x111;
    stackop2->size = 16;
    stackops.push_back(stackop2);

    StackOp stackop3 = new StackOperation();
    stackop3->stackOpType = STACK_WRITE;
    stackop3->stackAddr = 0x123;
    stackop3->offset = 0x113;
    stackop3->size = 16;
    stackops.push_back(stackop3);

    return stackops;
}

vector<MemOp> createMemopVector() {
    vector<MemOp> memops;
    MemOp memop0 = new MemOperation();
    memop0->memOpType = MALLOC;
    memop0->memAddr = 0x1234;
    memop0->size = 16;
    memops.push_back(memop0);

    MemOp memop1 = new MemOperation();
    memop1->memOpType = FREE;
    memop1->memAddr = 0x1234;
    memops.push_back(memop1);

    MemOp memop2 = new MemOperation();
    memop2->memOpType = STACK_FRAME_ALLOC;
    memop2->startMemAddr = 0x12897;
    memop2->endMemAddr = 0x11234;
    memop2->stackOperations = createStackOpVector();
    memop2->rtnName = "serialize_test";
    memops.push_back(memop2);

    MemOp memop3 = new MemOperation();
    memop3->memOpType = STACK_FRAME_FREE;
    memop3->memAddr = 0x11234;
    memop3->rtnName = "serialize_test";
    memops.push_back(memop3);

    return memops;
}

int main() {
    // create and open a character archive for output
    std::ofstream ofs("serializion");

    vector<MemOp> memops = createMemopVector();

    // save data to archive
    {
        boost::archive::text_oarchive oa(ofs);
        // write StackOperation instance to archive
        oa << memops;
    }

    // some time later restore the StackOperation instance to its orginal state
    vector<MemOp> newMemops;
    std::ifstream ifs("serializion");
    boost::archive::text_iarchive ia(ifs);
    // read state from archive
    ia >> newMemops;
    printMemOpVector(newMemops);

    return 0;
}

