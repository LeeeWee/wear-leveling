#include <vector>
#include <string>

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
    // memAddr malloced 
    UINT64 memAddr;
    // start address of stack memory allocated or freed.
    UINT64 startMemAddr;
    // end address of memory allocated or freed
    UINT64 endMemAddr;
    // memory size, unused if memOptype is FREE or STACK_FRAME_FREE
    UINT32 size;
    // stack operations in this stack frame, only invalid when memOpType is STACK_FRAME_ALLOC
    vector<StackOp> stackOperations;
    // operation name
    string rtnName;
};

typedef struct MemOperation * MemOp;

vector<MemOp> getMemoryOperations(const char *filename);