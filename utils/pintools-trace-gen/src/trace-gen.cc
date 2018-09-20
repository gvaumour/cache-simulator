#include <iostream>
#include <zlib.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include "pin.H"

using namespace std;

#define LINE_SIZE 50


/* Traces generation parameters*/  
#define WARMUP_WINDOW 100E6
#define RUNNING_WINDOW 200E9
#define REST_WINDOW 3E9

#define NUMBER_OF_WINDOWS 2
#define PREFIX_TRACE "memory_trace_"


enum State
{
	outROI,
	warming,
	running,
	resting,
};


State state;
int nb_windows;
uint64_t cpt_time;
uint64_t accessPhase;
char buffer[LINE_SIZE];

set<int> idThreads;

PIN_LOCK lock;
gzFile trace;

using namespace std;

VOID access(uint64_t pc , uint64_t addr, int type, int size, int id_thread)
{
	PIN_GetLock(&lock, id_thread);
	
	idThreads.insert(id_thread);
	
	if(state != outROI)
	{
		if(nb_windows < NUMBER_OF_WINDOWS)
		{
		
			if(state == resting && accessPhase == REST_WINDOW)
			{
				cout << "PINTOOLS:BEGIN OF INSTRUMENTATION" << endl;
				accessPhase = 0;		
				state = warming;
				std::string trace_str = string(PREFIX_TRACE) + to_string(nb_windows) + ".out";				
				trace = gzopen(trace_str.c_str(), "wb8");
			}
			else if(state == warming && accessPhase == WARMUP_WINDOW)
			{
				cout << "PINTOOLS:END OF WARMUP" << endl;
				state = running;
				accessPhase = 0;
			}
			else if(state == running && accessPhase == RUNNING_WINDOW)
			{
				state = resting;
				accessPhase = 0;
				
				nb_windows++;
				gzclose(trace);
				cout << "PINTOOLS:END OF INSTRUMENTATION" << endl;
			}
		
		
			if(state == running || state == warming)
			{
				sprintf(buffer, "#!0x%lx %d %d %d 0x%lx" , addr , type , size , id_thread, pc);
				gzwrite(trace, buffer, LINE_SIZE);
			}
			accessPhase++;	
		}
		else
		{
			PIN_ExitApplication(0);
		}
		cpt_time++;
	}
	PIN_ReleaseLock(&lock);
}



/* Record Instruction Fetch */
VOID RecordMemInst(VOID* pc, int size, int id_thread)
{
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);	
	access(convert_pc , convert_pc , 0 , size , id_thread);
}


/* Record Data Read Fetch */
VOID RecordMemRead(VOID* pc , VOID* addr, int size, int id_thread)
{
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);
	uint64_t convert_addr = reinterpret_cast<uint64_t>(addr);
	access(convert_pc , convert_addr , 1 , size , id_thread);
}


/* Record Data Write Fetch */
VOID RecordMemWrite(VOID * pc, VOID * addr, int size, int id_thread)
{
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);
	uint64_t convert_addr = reinterpret_cast<uint64_t>(addr);
	access(convert_pc , convert_addr , 2 , size , id_thread);
}

VOID Routine(RTN rtn, VOID *v)
{           
	RTN_Open(rtn);
	
	for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemInst,
		    IARG_INST_PTR,
		    IARG_UINT32,
		    INS_Size(ins),
		    IARG_THREAD_ID,
		    IARG_END);

    		UINT32 memOperands = INS_MemoryOperandCount(ins);	    						
	
		for (UINT32 memOp = 0; memOp < memOperands; memOp++){
			if (INS_MemoryOperandIsRead(ins, memOp))
			{
				INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
				IARG_INST_PTR,
				IARG_MEMORYOP_EA, memOp,
				IARG_MEMORYREAD_SIZE,
				IARG_THREAD_ID,
				IARG_END);
			}

			if (INS_MemoryOperandIsWritten(ins, memOp))
			{
				INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
				IARG_INST_PTR,
				IARG_MEMORYOP_EA, memOp,
				IARG_MEMORYREAD_SIZE,
				IARG_THREAD_ID,
				IARG_END);
			}
		}
	}
	RTN_Close(rtn);
}


VOID Fini(INT32 code, VOID *v)
{
	cout << "PINTOOLS:NB Access " << cpt_time << endl;
	cout << "PINTOOLS:NB Threads " << idThreads.size() << endl;
//	cout << ""
//	if(state != outROI)
//	{
	gzclose(trace);	
//	}
	
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
	if (PIN_Init(argc, argv)) return Usage();


	PIN_InitLock(&lock);

	nb_windows=0;
	cpt_time =0;
	accessPhase = 0;

	RTN_AddInstrumentFunction(Routine, 0);
	PIN_AddFiniFunction(Fini, 0);
	
	state = running;
			
	PIN_StartProgram();

	return 0;
}
