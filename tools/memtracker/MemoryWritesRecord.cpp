#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cctype>
#include <map>
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


/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

std::ofstream outFile;

bool go = false;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "MemoryWritesRecord.out", "specify memory writes record file name");

/* ===================================================================== */

/* 
 * Callbacks on alloc functions are not working properly before main() is called.
 * We sometimes get a callback for function exit without having received the callback
 * on function enter. So we don't track anything before main() is called. 
 */
VOID callBeforeMain()
{
	outFile << "MAIN CALLED ++++++++++++++++++++++++++++++++++++++++++" << endl;
    go = true;
}

VOID callAfterMain()
{
    outFile << "MAIN Exited ++++++++++++++++++++++++++++++++++++++++++" << endl;
    go = false;
}

VOID callBeforeMalloc(ADDRINT size)
{
    go = true;
    outFile << MALLOC << ", size: " << size << endl;
}

VOID callAfterMalloc(ADDRINT ret)
{
    go = false;
    outFile << "Exit Routine: " << MALLOC << endl;
    outFile << MALLOC << " return: " << ret << endl << endl; 
}

VOID callBeforeFREE(ADDRINT addr)
{
    go = true;
    outFile << FREE << ", address: " << addr << endl;
}

VOID callAfterFREE()
{
    go = false;
    outFile << "Exit Routine: " << FREE << endl << endl;
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
    outFile << "Entry Routine: " << rtnName << endl;
}

VOID RTNExit(CHAR * rtnName) 
{
    if (!go)
        return;
    outFile << "Exit Routine: " << rtnName << endl;
}

VOID MemWriteBefore(ADDRINT ip, ADDRINT addr, ADDRINT size, ADDRINT insarg, BOOL isStackWrite, BOOL isIpRelWrite) 
{
    if (!go)
        return;

    INS ins;
    ins.q_set(insarg);
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


/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
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
