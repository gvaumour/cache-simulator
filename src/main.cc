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
#include <iomanip>
#include <signal.h>
#include <unistd.h>

#include "main.hh"
#include "Hierarchy.hh"
#include "MemoryTrace.hh"
#include "common.hh"

#define MERGING_RESULTS true

using namespace std;

int start_debug;
Hierarchy* my_system;
vector<string> memory_traces;

int main(int argc , char* argv[]){
	
	vector<string> args; 

	bool mergingResults = MERGING_RESULTS;
	init_default_parameters();
	
	for(int i = 1; i < argc ; i++)
	{
		if(string(argv[i]) == "-p")
		{	i++;
			simu_parameters.policy = string(argv[i]);		
		}
		else if(string(argv[i]) == "--debug-flags")
		{	i++;
			vector<string> debugflags = split(string(argv[i]) , ',');		
			for(auto debugflag : debugflags)
			{
				if(simulation_debugflags.count(debugflag) != 0)
					simu_parameters.enable_debugflags.insert(debugflag);
				else
				{
					cout << "Error: Debug flag not recognized : " << debugflag << endl;
					return 0;					
				}
			}
		}
		else if(string(argv[i]) == "-n")
		{	i++;
			simu_parameters.nbCores = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--rap-assoc")
		{	i++;
			simu_parameters.rap_assoc = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--rap-sets")
		{	i++;
			simu_parameters.rap_sets = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--window_size")
		{	i++;
			simu_parameters.window_size = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--SRAM-assoc")
		{	i++;
			simu_parameters.sram_assoc = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--NVM-assoc")
		{	i++;
			simu_parameters.nvm_assoc = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--ratio-RWcost")
		{	i++;
			simu_parameters.ratio_RWcost = atof(argv[i]);
		}
		else if(string(argv[i]) == "--MT-size")
		{	i++;
			simu_parameters.sizeMTtags = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--InstructionTh")
		{	i++;
			simu_parameters.cost_threshold = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--SaturationTh")
		{	i++;
			simu_parameters.saturation_threshold = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--LLC-sets")
		{	i++;
			simu_parameters.nb_sets = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--deadCounter")
		{	i++;
			simu_parameters.deadSaturationCouter = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--prefetchDegree")
		{	i++;
			simu_parameters.prefetchDegree = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--prefetchStreams")
		{	i++;
			simu_parameters.prefetchStreams = atoi(argv[i]);
		}
		else if(string(argv[i]) == "--enablePrefetch")
			simu_parameters.enablePrefetch = true;
		else if(string(argv[i]) == "--traceLLC")
			simu_parameters.traceLLC = true;
		else if(string(argv[i]) == "--enablePCHistoryTracking")
			simu_parameters.enablePCHistoryTracking = true;
		else if(string(argv[i]) == "--enableBP")
			simu_parameters.enableBP = true;
		else if(string(argv[i]) == "--enable-DYN")
			simu_parameters.enableMigration = true;
		else if(string(argv[i]) == "--simulate-MissConflicts")
			simu_parameters.simulate_conflicts = true;
		else if(string(argv[i]) == "--enableReuseError")
			simu_parameters.enableReuseErrorComputation = true;
		else if(string(argv[i]) == "--Predictor-optTarget")
		{	i++;
			string m_optTarget = string(argv[i]);
			assert((m_optTarget == "energy" || m_optTarget == "perf") && "Wrong optimization parameter for the function cost" );
			if(m_optTarget == "energy")
				simu_parameters.optTarget = EnergyParameters();
			else if(m_optTarget == "perf")
				simu_parameters.optTarget = PerfParameters();
		}
		else if(string(argv[i]) == "--DBAMB-signature")
		{	i++;
			simu_parameters.DBAMB_signature = string(argv[i]);		
		}
		else if(string(argv[i]) == "--Perceptron-Features")
		{	i++;
			simu_parameters.perceptron_features.clear();
			simu_parameters.perceptron_features = split(string(argv[i]) , ',');
		}
		else if(string(argv[i]) == "--Perceptron-drawFeatureMaps")
			simu_parameters.perceptron_drawFeatureMaps = true;		
		else if(string(argv[i]) == "--Perceptron-BPThreshold")
		{	i++;
			simu_parameters.perceptron_threshold_bypass = atoi(argv[i]);		
		}
		else if(string(argv[i]) == "--Perceptron-BPLearning")
		{	i++;
			simu_parameters.perceptron_threshold_learning = atoi(argv[i]);		
		}
		else if(string(argv[i]) == "--DBAMB-addr-beginBits")
		{	i++;
			simu_parameters.DBAMB_begin_bit = atoi(argv[i]);		
		}
		else if(string(argv[i]) == "--DBAMB-addr-endBits")
		{	i++;
			simu_parameters.DBAMB_end_bit = atoi(argv[i]);		
		}
		else if(string(argv[i]) == "--flagTest")
		{
			simu_parameters.flagTest = true;
			cout << "Flag Test Enable" << endl;		
		}
		else if(string(argv[i]) == "--readDatasetFile")
		{
			i++;
			simu_parameters.readDatasetFile = true;
			simu_parameters.datasetFile = string(argv[i]);		
		}
		else if(string(argv[i]) == "--writeDatasetFile")
			simu_parameters.writeDatasetFile = true;
		else if(string(argv[i]) == "--enableDebug")
		{
			cout << "Debug enable "<< endl;		
			simu_parameters.printDebug = true;
		}
		else
			args.push_back(string(argv[i]));
	}
	
	if(simu_parameters.policy == "DBA")
	{
		simu_parameters.enableBP = false;
		simu_parameters.enableMigration = false;
	}
	else if(simu_parameters.policy == "DBAMB")
	{
		simu_parameters.enableBP = true;
		simu_parameters.enableMigration = true;
	}
	
	if( (simu_parameters.nvm_assoc == 0 || simu_parameters.sram_assoc == 0) && simu_parameters.policy != "Perceptron" )
	 {
	 	simu_parameters.policy = "LRU";
	 }
	
	for(unsigned i = 0 ; i < args.size() ; i++)
		memory_traces.push_back(args[i]);		
	
	if(argc == 1)
		memory_traces.push_back(DEFAULT_TRACE);
	
	
	
	/* The Control C signal handler setup */
	/*struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = my_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);*/
	/***********************************/	

	my_system = new Hierarchy(simu_parameters.policy , simu_parameters.nbCores);
 
	cout << "Launching simulation with " << memory_traces.size() << " file(s), the " << simu_parameters.policy \
	 << " policy and with " << simu_parameters.nbCores << " core(s)" << endl;
	cout << "Traces considered:" << endl;
	for(auto memory_trace : memory_traces)
		cout << "\t - " << memory_trace << endl;
	
	int id_trace = 0;
	string loading_bar = "";
	uint64_t step = 300E6/30;

	for(auto memory_trace : memory_traces)
	{
		cout << "\tRunning Trace: " << memory_trace << " ... " <<  endl;
	
		MemoryTrace* traceWrapper = initTrace(memory_trace);
		assert(traceWrapper != NULL);
		
		Access element;
		uint64_t cpt_access = 0;
		my_system->startWarmup();
		my_system->stopWarmup();
		
		while(traceWrapper->readNext(element)){

			cpt_time++;
			cpt_access++;

			if( (cpt_time-1) % step == 0)
			{
				cout << "\tSimulation Progress [" << std::left << setw(30) << setfill(' ') \
					<< loading_bar << "] " << setw(2) << loading_bar.size()*100/30 << "%\r";
				fflush(stdout);
				loading_bar += "#";				
			}


			my_system->handleAccess(element);
			DPRINTF(DebugCache, "TIME::%ld\n",cpt_access);
			
		}
		cout << endl;
		traceWrapper->close();
		free(traceWrapper);
		if(!mergingResults)
		{
			my_system->finishSimu();	
			printResults(mergingResults, id_trace);	
		
		}
		id_trace++;
	}	
	if(mergingResults)
	{
		my_system->finishSimu();	
		printResults(mergingResults , id_trace);	
	}
	
	delete(my_system);
	return 0;
}


void my_handler(int s){
	cout << endl;
	cout << "Ctrl+C event caught, printing results and exiting" << endl;
	
	printResults(false , 0);
		
	exit(1); 

}


/** Writing all the output+config files */ 
void printResults(bool mergingResults, int id_trace)
{
	if(!my_system)
		return;
		
		
	string suffixe = mergingResults ? "_"+to_string(id_trace) : ""; 
	
	ofstream configFile;
	configFile.open("config" + suffixe + ".ini");
	
	configFile << "Simulated traces:" << endl;
	for(auto p : memory_traces)
		configFile << "\t-" << p << endl;
 	
	my_system->printConfig(configFile);
	configFile.close();
	
	ofstream output_file;
	output_file.open("results"+ suffixe + ".out");
	my_system->printResults(output_file);
	output_file.close();
}


