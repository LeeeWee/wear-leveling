#ifndef MEMOPCOLLECTOR_H
#define MEMOPCOLLECTOR_H

#include <vector>
#include <string>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

using namespace std;

#define UINT16 short unsigned int
#define UINT32 unsigned int
#define UINT64 unsigned long long

// stack operations, include stack write and stack read
typedef enum StackOpType {
    STACK_WRITE,
    STACK_READ
}StackOpType;

struct StackOperation {
    StackOpType stackOpType; // stack operation type
    UINT64 stackAddr; // stack address of stack operation
    UINT32 offset; // offset to the end address of stack frame
    UINT16 size;
};

typedef struct StackOperation * StackOp;


typedef enum MemOpType {
    MALLOC,
    FREE,
    STACK_FRAME_ALLOC,
    STACK_FRAME_FREE,
}MemOpType;

struct MemOperation {
    // memory operation type
    MemOpType memOpType;
    // memAddr malloced or freed (malloc/malloc_free/stackmem_free)
    UINT64 memAddr;
    // start address of stack memory allocated
    UINT64 startMemAddr;
    // end address of stack memory allocated
    UINT64 endMemAddr;
    // memory size, unused if memOptype is FREE or STACK_FRAME_FREE
    UINT32 size;
    // stack operations in this stack frame, only invalid when memOpType is STACK_FRAME_ALLOC
    vector<StackOp> stackOperations;
    // operation name (stack memory alloc or free)
    string rtnName;
};

typedef struct MemOperation * MemOp;

// use the boost serialization to mean the reversible deconstruction of an arbitrary set of C++ data structures to a sequence of bytes
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

vector<MemOp> getMemoryOperations(const char *filename);

void printMemOperations(vector<MemOp> memoryOperations);

void wearCount(vector<MemOp> memoryOperations, UINT32 stackWearCounter[], UINT32 heapWearCounter[],
                UINT64 &startAddrOfStack, UINT64 &startAddrOfHeap);

void printWearCount(UINT32 wearCount[], int size, UINT64 startAddr, bool isStack);

// save memory operations to binary file
void saveMemoryOperations(vector<MemOp> memoryOperations, const char *filename);

// load memory operations from binary file
vector<MemOp> loadMemoryOperations(const char *filename);

#endif