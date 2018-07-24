/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2016 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT0,
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pin.H"

// Output the result into this file.
FILE * trace;

// Names of malloc
#if defined(TARGET_MAC)
#define MALLOC "_malloc"
#define FREE "_free"
#else
#define MALLOC "malloc"
#define FREE "free"
#endif

long unsigned int firstMalloc = 0;
int i = 0;
long unsigned int mallocSize = 0;

unsigned calculate(long unsigned int tmp) {
    unsigned size = 0;
    while(tmp != 0) {
      tmp = tmp / 16;
      size++;
    }
    return size;
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr)
{
    long unsigned int tempAddr;
    tempAddr = (long unsigned int)addr;
    
    unsigned AddrSize = calculate(tempAddr);
   
    if(AddrSize == 7){
      if(firstMalloc != 0 && tempAddr > firstMalloc){
        fprintf(trace, "%lx\n", tempAddr);
      }
    }

    if(AddrSize == 6 && ((tempAddr / 100000) >= 7)){
      if(firstMalloc != 0 && tempAddr > firstMalloc){
        fprintf(trace, "%lx\n", tempAddr);
      }
    }
}

VOID RecordMallocSize(CHAR * name, ADDRINT size)
{
    long unsigned int tempSize;
    tempSize = (long unsigned int)size;
    //fprintf(trace, "%s(%ld)\n", name, tempSize);
    mallocSize = tempSize;
}

VOID RecordMallocVal(ADDRINT ret)
{
    i++;   
    long unsigned int int_ret;
    int_ret = (long unsigned int)ret;

    if(i == 1){
      firstMalloc = int_ret;
      fprintf(trace,"First malloc: %lx \n", firstMalloc);
    }
    //fprintf(trace, "return(%lx)\n", int_ret);
}

// Instrument Routine   
VOID Image(IMG img, VOID *v)
{
    // Instrument the malloc() functions.  Print the return value of malloc().
    //   
    //  Find the malloc() function.
    RTN mallocRtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)RecordMallocSize,
                       IARG_ADDRINT, MALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);

        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)RecordMallocVal,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(mallocRtn);
    }
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        // Note that in some architectures a single memory operand can be 
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_END);
        }
    }
}

VOID Fini(INT32 code, VOID *v)
{
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin & symbol manager
    PIN_InitSymbols();    

    if (PIN_Init(argc, argv)) return Usage();

    trace = fopen("malloctrace_test.out", "w");

    // Register Image to be called to instrument functions.
    IMG_AddInstrumentFunction(Image, 0);

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
