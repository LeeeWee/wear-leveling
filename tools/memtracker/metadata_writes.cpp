#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cctype>
#include <map>
#include <climits>
#include "pin.H"


/* ===================================================================== */
/* Names of malloc and free */
/* ===================================================================== */
#ifndef MALLOC
#define MALLOC "basemalloc"
#endif
#ifndef FREE
#define FREE "basefree"
#endif

#ifndef UINT64
#define UINT64 unsigned long long
#endif

// the minimal size of memory 
#define ALIGNMENT 8 
// block size for calculating wear info, must be a power of ALIGNMENT
#define BLOCK_SIZE 64

struct MetadataWrite{
    UINT64 address;
    UINT32 size;
};

typedef struct MetadataWrite * MdWrite;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

std::ofstream outFile;

bool go = false;


// record the metadata writes
std::vector<MdWrite> metadataWrites;


/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "metadata_writes.out", "collect metadata writes");

/* ===================================================================== */

/* 
 * Callbacks on alloc functions are not working properly before main() is called.
 * We sometimes get a callback for function exit without having received the callback
 * on function enter. So we don't track anything before main() is called. 
 */
VOID callBeforeMain()
{
#ifdef DEBUG
	outFile << "MAIN CALLED ++++++++++++++++++++++++++++++++++++++++++" << endl;
#endif
    go = true;
}

VOID callAfterMain()
{
#ifdef DEBUG
    outFile << "MAIN Exited ++++++++++++++++++++++++++++++++++++++++++" << endl;
#endif
    go = false;
}

VOID callBeforeMalloc(ADDRINT size)
{
    go = true;
#ifdef DEBUG
    outFile << MALLOC << ", size: " << size << endl;
#endif
}

VOID callAfterMalloc(ADDRINT ret)
{
    go = false;
#ifdef DEBUG
    outFile << "Exit Routine: " << MALLOC << endl;
    outFile << MALLOC << " return: " << ret << endl << endl; 
#endif
}

VOID callBeforeFREE(ADDRINT addr)
{
    go = true;
#ifdef DEBUG
    outFile << FREE << ", address: " << addr << endl;
#endif
}

VOID callAfterFREE()
{
    go = false;
#ifdef DEBUG
    outFile << "Exit Routine: " << FREE << endl << endl;
#endif
}

const char * StripPath(const char * path) 
{
    const char * file = strrchr(path, '/');
    if (file) 
        return file + 1;
    else
        return path;
}

VOID RTNEntry(CHAR * rtnName)
{
    if (!go)
        return;
#ifdef DEBUG
    outFile << "Entry Routine: " << rtnName << endl;
#endif
}

VOID RTNExit(CHAR * rtnName) 
{
    if (!go)
        return;
#ifdef DEBUG
    outFile << "Exit Routine: " << rtnName << endl;
#endif
}

VOID MemWriteBefore(ADDRINT ip, CHAR * rtnName, ADDRINT addr, ADDRINT size, ADDRINT insarg, BOOL isStackWrite, BOOL isIpRelWrite) 
{
    if (!go)
        return;

    if (strcmp(rtnName, "firstfit_block") == 0 || strcmp(rtnName, "nextfit_block") == 0 || strcmp(rtnName, "bestfit_block") == 0)
        return;

    if (!isStackWrite && !isIpRelWrite) {
        INS ins;
        ins.q_set(insarg);
#ifdef DEBUG
        outFile << setw(16) << left << ip;
        outFile << setw(7)  << left << "write";
        outFile << setw(16) << left << addr ;
        outFile << setw(10) << left << size;
        outFile << setw(50) << left << INS_Disassemble(ins);
        if (isStackWrite)
            outFile << "isStackWrite  ";
        else 
            outFile << "NoStackWrite  ";
        if (isIpRelWrite)
            outFile << "isIpRelWrite" << endl;
        else 
            outFile << "NOIpRelWrite" << endl;
#endif

        // add MemWriteRec to vector
        MdWrite mdwrite = new MetadataWrite();
        mdwrite->address = addr;
        mdwrite->size = size;
        // push back to records and metadataWrites vector
        metadataWrites.push_back(mdwrite);
    } 
}


/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */
   
VOID Image(IMG img, VOID *v)
{
    // Find main. We won't do anything before main starts.
    // RTN rtn = RTN_FindByName(img, "main");
    // if (RTN_Valid(rtn))
    // {
    //     RTN_Open(rtn);
    //     RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)callBeforeMain, IARG_END);
    //     RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)callAfterMain, IARG_END);
    //     RTN_Close(rtn);
    // }

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {     
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            RTN_Open(rtn);
            const char * rtnName = StripPath(RTN_Name(rtn).c_str());

            if (strcmp(rtnName, MALLOC) == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)callBeforeMalloc, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)callAfterMalloc, IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
            }

            if (strcmp(rtnName, FREE) == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)callBeforeFREE, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)callAfterFREE, IARG_END);
            }

            RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)RTNEntry,
                    IARG_ADDRINT, rtnName, IARG_END);
            RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)RTNExit,
                    IARG_ADDRINT, rtnName, IARG_END);

            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                UINT32 memOperands = INS_MemoryOperandCount(ins);
                for (UINT32 memOp = 0; memOp < memOperands; memOp++)
                {    
                    const UINT32 size = INS_MemoryOperandSize(ins, memOp);
                    if (INS_MemoryOperandIsWritten(ins, memOp))
                    {
                        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemWriteBefore, 
                            IARG_INST_PTR, 
                            IARG_ADDRINT, rtnName,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_ADDRINT, size,
                            IARG_ADDRINT, ins.q(),
                            IARG_BOOL, INS_IsStackWrite(ins),
                            IARG_BOOL, INS_IsIpRelWrite(ins), IARG_END);
                    }
                }
            }
            
            RTN_Close(rtn);
        } 
    }
}


void saveMetadataWrites() {
    std::vector<MdWrite>::iterator iter;
    for (iter = metadataWrites.begin(); iter != metadataWrites.end(); iter++) {
        outFile << std::hex;
        outFile << "metadata address: " << (*iter)->address << ", size: " << (*iter)->size << std::endl;
    }
}


/* ===================================================================== */
/* WearCount                                                               */
/* ===================================================================== */
/**
void calculateWearCount() {
    wearcount_size = (endOfMemory - startOfMemory) / ALIGNMENT;
    blocks_wearcount_size = (endOfMemory - startOfMemory) % BLOCK_SIZE  == 0 ? 
        (endOfMemory - startOfMemory) / BLOCK_SIZE : (endOfMemory - startOfMemory) / BLOCK_SIZE + 1;
    // first count wear by the size of ALOGNMENT
    // metadata
    metadata_wearcount = new int[wearcount_size]();
    std::vector<MemWritesRec>::iterator iter;
    for (iter = metadataWrites.begin(); iter != metadataWrites.end(); iter++) {
        int startLineNo = ((*iter)->address - startOfMemory) / ALIGNMENT;
        int endLineNo = ((*iter)->address + (*iter)->size - startOfMemory) % ALIGNMENT == 0 ?
            ((*iter)->address + (*iter)->size - startOfMemory) / ALIGNMENT - 1 : ((*iter)->address + (*iter)->size - startOfMemory) / ALIGNMENT;
#ifdef DEBUG
        outFile << "metadata_write, address: " << (*iter)->address << ", size: " << (*iter)->size <<  ", startLineNo: " << startLineNo << ", endLineNo: " << endLineNo << endl;
#endif
        for (int i = startLineNo; i <= endLineNo; i++) {
            metadata_wearcount[i] += 1;
        }
    }
    //data
    data_wearcount = new int[wearcount_size]();
    for (iter = dataWrites.begin(); iter != dataWrites.end(); iter++) {
        int startLineNo = ((*iter)->address - startOfMemory) / ALIGNMENT;
        int endLineNo = ((*iter)->address + (*iter)->size - startOfMemory) % ALIGNMENT == 0 ?
            ((*iter)->address + (*iter)->size - startOfMemory) / ALIGNMENT - 1 : ((*iter)->address + (*iter)->size - startOfMemory) / ALIGNMENT;
#ifdef DEBUG
        outFile << "data_write, address: " << (*iter)->address << ", size: " << (*iter)->size <<  ", startLineNo: " << startLineNo << ", endLineNo: " << endLineNo << endl;  
#endif
        for (int i = startLineNo; i <= endLineNo; i++) {
            data_wearcount[i] += 1;
        }
    }
    // calculate the sum of wear by the size of BLOCK_SIZE
    // metadata
    int times = BLOCK_SIZE / ALIGNMENT;
    metadata_blocks_wearcount = new int[blocks_wearcount_size]();
    for (int j = 0; j < blocks_wearcount_size; j++) {
        for (int k = j * times; k < (j + 1) * times; k++) {
            if (k >= wearcount_size)
                break;
            if (metadata_wearcount[k] > metadata_blocks_wearcount[j]) 
                metadata_blocks_wearcount[j] = metadata_wearcount[k];
        }
    }
    // data
    data_blocks_wearcount = new int[blocks_wearcount_size]();
    for (int j = 0; j < blocks_wearcount_size; j++) {
        for (int k = j * times; k < (j + 1) * times; k++) {
            if (k >= wearcount_size)
                break;
            if (data_wearcount[k] > data_blocks_wearcount[j])
                data_blocks_wearcount[j] = data_wearcount[k];
        }
    }
}
*/

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
    outFile << dec;
    saveMetadataWrites();
    
    outFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    cerr << "This tool produces a trace of calls to memory allocate." << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}



/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin & symbol manager
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    // Write to a file since cout and cerr maybe closed by the application
    outFile.open(KnobOutputFile.Value().c_str());
    outFile << hex;
    outFile.setf(ios::showbase);

    
    // Register Image to be called to instrument functions.
    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
} 


/* ===================================================================== */
/* eof */
/* ===================================================================== */
