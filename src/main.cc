/** 
Copyright (C) 2016 Gregory Vaumourin

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/

#include <stdlib.h>     
#include <ctime>
#include <vector>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <string>
#include <zlib.h>
#include <signal.h>
#include <unistd.h>

#include "main.hh"
#include "Hierarchy.hh"
#include "MemoryTrace.hh"
#include "common.hh"

using namespace std;

int start_debug;
Hierarchy* my_system;

int main(int argc , char* argv[]){

	vector<string> memory_traces;
	for(int i = 1 ; i < argc ; i++)
		memory_traces.push_back(string(argv[i]));		
	
	if(argc == 1)
		memory_traces.push_back(DEFAULT_TRACE);

	
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = my_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	

	my_system = new Hierarchy("testRAP" , 4);
 
 
	cout << "Launching simulation with " << memory_traces.size() << "file(s)" << endl;
	for(auto memory_trace : memory_traces)
	{
		cout << "\tRunning Trace: " << memory_trace << " ... " <<  endl;
	
		MemoryTrace* traceWrapper = initTrace(memory_trace);
		assert(traceWrapper != NULL);
		
		Access element;
		int cpt_access = 0;

		my_system->startWarmup();
		
		while(traceWrapper->readNext(element)){

			my_system->handleAccess(element);
			
			if(cpt_access == WARMUP_WINDOW)
			{
				cout << "\t\tFinished the warmup phase" << endl;			
				my_system->stopWarmup();
			}
			
			cpt_access++;
		}
		
		traceWrapper->close();
		free(traceWrapper);
	}		
	printResults();
	
	delete(my_system);
	return 0;
}


void my_handler(int s){
	cout << endl;
	cout << "Ctrl+C event caught, printing results and exiting" << endl;
	
	printResults();
		
	exit(1); 

}


/** Writing all the output+config files */ 
void printResults()
{

	ofstream configFile;
	configFile.open(CONFIG_FILE);
	my_system->printConfig(configFile);
	configFile.close();
	
	ofstream output_file;
	output_file.open(OUTPUT_FILE);
	my_system->printResults(output_file);
	output_file.close();
}


