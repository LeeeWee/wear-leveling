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
#if defined(TARGET_MAC)
#define MALLOC "_malloc"
#define FREE "_free"
#else
#define MALLOC "malloc"
#define FREE "free"
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
    "o", "memtracker.out", "specify trace file name");

/* ===================================================================== */


/* 
 * Callbacks on alloc functions are not working properly before main() is called.
 * We sometimes get a callback for function exit without having received the callback
 * on function enter. So we don't track anything before main() is called. 
 */
VOID callBeforeMain()
{
	outFile << "MAIN CALLED ++++++++++++++++++++++++++++++++++++++++++" << endl;
    // outFile << setw(16) << left << "ins_ptr";
    // outFile << setw(7)  << left << "w/r";
    // outFile << setw(16) << left << "oper_addr";
    // outFile << setw(10) << left << "oper_size";
    // outFile << setw(50) << left << "instruction";
    // outFile << setw(16) << left << "stack_address";
    // outFile << setw(12) << left << "rbp_address" << endl;
    go = true;
}

VOID callAfterMain()
{
    outFile << "MAIN Exited ++++++++++++++++++++++++++++++++++++++++++" << endl;
    go = false;
}


/* ===================================================================== */
/* Analysis malloc */
/* ===================================================================== */
 
VOID MallocBefore(ADDRINT size, CHAR * imageName)
{
    if (!go)
        return;
    // outFile << MALLOC << "(" << size << ")" << endl;
    outFile << MALLOC << " " << size << endl;
}

VOID MallocAfter(ADDRINT ret)
{
    if (!go)
        return;
    outFile << MALLOC << " returns " << ret << endl;
}

VOID FreeBefore(ADDRINT ip)
{
    if (!go)
        return;
    // outFile << FREE << "(" << ip << ")" << endl;
    outFile << FREE << " " << ip << endl;
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
    // outFile << "<<<<<<<<<<<< Entry " << rtnName << " <<<<<<<<<<<<<<" << endl;
    outFile << "Entry " << rtnName << " <<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
}

VOID RTNExit(CHAR * rtnName) 
{
    if (!go)
        return;
    // outFile << "<<<<<<<<<<<< Exit " << rtnName << " <<<<<<<<<<<<<<" << endl;
    outFile << "Exit " << rtnName << " <<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
}

/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */
   
VOID Image(IMG img, VOID *v)
{
    // Find main. We won't do anything before main starts.
    RTN rtn = RTN_FindByName(img, "main");
    if (RTN_Valid(rtn))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)callBeforeMain, IARG_END);
        // RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)callAfterMain, IARG_END);
        RTN_Close(rtn);
    }

    // iterate all rtn
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {     
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            // printf ("  Rtn: %s\n", RTN_Name(rtn).c_str());
            RTN_Open(rtn);
            const char * rtnName = StripPath(RTN_Name(rtn).c_str());
            RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)RTNEntry,
                       IARG_ADDRINT, rtnName, IARG_END);
            RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)RTNExit,
                       IARG_ADDRINT, rtnName, IARG_END);
            RTN_Close(rtn);
        } 
    }

    // Instrument the malloc() and free() functions.  Print the input argument
    // of each malloc() or free(), and the return value of malloc().
    //
    //  Find the malloc() function.
    RTN mallocRtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

        const char * imageName = StripPath(IMG_Name(SEC_Img(RTN_Sec(mallocRtn))).c_str());
        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)MallocBefore,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_ADDRINT, imageName,
                       IARG_END);
        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)MallocAfter,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(mallocRtn);
    }

    // Find the free() function.
    RTN freeRtn = RTN_FindByName(img, FREE);
    if (RTN_Valid(freeRtn))
    {
        RTN_Open(freeRtn);
        // Instrument free() to print the input argument value.
        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)FreeBefore,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(freeRtn);
    }
}

VOID MemWriteBefore(ADDRINT ip, ADDRINT addr, ADDRINT size, ADDRINT insarg, ADDRINT sp, ADDRINT bp, BOOL stackOp) 
{
    if (!go)
        return;

    if (stackOp) // only print stack operation
    {
        INS ins;
        ins.q_set(insarg);
        // outFile << setw(16) << left << ip;
        // outFile << setw(7)  << left << "write";
        // outFile << setw(16) << left << addr ;
        // outFile << setw(10) << left << size;
        // outFile << setw(50) << left << INS_Disassemble(ins);
        // outFile << setw(16) << left << sp;
        // outFile << setw(16) << left << bp;
        // outFile << endl;
        outFile << "write " << addr << " " << size << endl;
    }
    // else
    //     outFile << endl;
        // outFile << "write" << "(" << addr << "), size: " << size << endl;
}

VOID MemReadBefore(ADDRINT ip, ADDRINT addr, ADDRINT size, ADDRINT insarg, ADDRINT sp, ADDRINT bp, BOOL stackOp) 
{
    if (!go)
        return;
    if (stackOp) // only print stack operation 
    {
        INS ins;
        ins.q_set(insarg);
        // outFile << setw(16) << left << ip;
        // outFile << setw(7) << left << "read";
        // outFile << setw(16) << left << addr ;
        // outFile << setw(10) << left << size;
        // outFile << setw(50) << left << INS_Disassemble(ins);
        // outFile << setw(16) << left << sp;
        // outFile << setw(16) << left << bp;
        // outFile << endl;
        outFile << "read " << addr << " " << size << endl;
    }
    // else
    //     outFile << endl;
        // outFile << "read" << "(" << addr << "), size: " << size << endl;
}

VOID StackRegSubBefore(ADDRINT ip, ADDRINT insarg, ADDRINT sp) 
{
    if (!go)
        return;
    INS ins;
    ins.q_set(insarg);
    outFile << " ======> sub stack register: " << ip << " "<< INS_Disassemble(ins) << endl;
    outFile << " ======> " << sp << " ";
}

VOID StackRegSubAfter(ADDRINT sp) 
{
    if (!go)
        return;
    outFile << sp << endl;
}

VOID StackRegAddBefore(ADDRINT ip, ADDRINT insarg, ADDRINT sp) 
{
    if (!go)
        return;
    INS ins;
    ins.q_set(insarg);
    outFile  << " ======> add stack register: " << ip << " " << INS_Disassemble(ins) << endl;
    outFile << " ======> " << sp << " ";
}

VOID StackRegAddAfter(ADDRINT sp) 
{
    if (!go)
        return;
    outFile << sp << endl;
}


/* ===================================================================== */
/* Instrumentation insstructions                                         */
/* ===================================================================== */
VOID Instruction(INS ins, VOID *v)
{
    /*
    if (INS_RegWContain(ins, REG_STACK_PTR)) {
        if (INS_IsSub(ins))
        {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StackRegSubBefore,
                IARG_INST_PTR,
                IARG_ADDRINT, ins.q(),
                IARG_REG_VALUE, REG_STACK_PTR, IARG_END);
            INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)StackRegSubAfter,
                IARG_REG_VALUE, REG_STACK_PTR, IARG_END);
        }
        if (INS_Opcode(ins) == XED_ICLASS_ADD) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StackRegAddBefore,
                IARG_INST_PTR,
                IARG_ADDRINT, ins.q(),
                IARG_REG_VALUE, REG_STACK_PTR, IARG_END);
            INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)StackRegAddAfter,
                IARG_REG_VALUE, REG_STACK_PTR, IARG_END);
        }
    } 
    */

    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Instrument each memory operand. If the operand is both read and written
    // it will be processed twice.
    // Iterating over memory operands ensures that instructions on IA-32 with
    // two read operands (such as SCAS and CMPS) are correctly handled.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        const UINT32 size = INS_MemoryOperandSize(ins, memOp);
        // const BOOL   single = (size <= 4);
        
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemReadBefore,
                IARG_INST_PTR, 
                IARG_MEMORYOP_EA, memOp,
                IARG_ADDRINT, size, 
                IARG_ADDRINT, ins.q(),
                IARG_REG_VALUE, REG_STACK_PTR,
                IARG_REG_VALUE, REG_GBP,
                IARG_BOOL, INS_IsStackRead(ins), IARG_END);
        }

        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemWriteBefore, 
                IARG_INST_PTR, 
                IARG_MEMORYOP_EA, memOp,
                IARG_ADDRINT, size,
                IARG_ADDRINT, ins.q(),
                IARG_REG_VALUE, REG_STACK_PTR,
                IARG_REG_VALUE, REG_GBP,
                IARG_BOOL, INS_IsStackWrite(ins), IARG_END);
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
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
} 


/* ===================================================================== */
/* eof */
/* ===================================================================== */
