#ifndef ROEVAL_HH_
#define ROEVAL_HH_

#define DEFAULT_TRACE "/home/gvaumour/Dev/apps/pin/parsec_traces/raytrace/memory_trace_0.out"
//#define DEFAULT_TRACE "trace_test.txt"
#define WARMUP_WINDOW 100E6

#include "Hierarchy.hh"

void printResults(bool mergingResults, int id_trace);
void my_handler(int s);


#endif
