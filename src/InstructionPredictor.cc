#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "InstructionPredictor.hh"

using namespace std;

/** InstructionPredictor Implementation ***********/ 

/*
InstructionPredictor::InstructionPredictor() : Predictor(){
	m_cpt = 1;
}*/

InstructionPredictor::InstructionPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;
}
		
InstructionPredictor::~InstructionPredictor()
{ }

allocDecision
InstructionPredictor::allocateInNVM(uint64_t set, Access element)
{
	DPRINTF("InstructionPredictor::allocateInNVM\n");
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;

	if(pc_counters.count(element.m_pc) == 0){
		//New PC to track		
		pc_counters.insert(pair<uint64_t,int>(element.m_pc, 1));	

		if(!m_isWarmup)
			stats_PCwrites.insert(pair<uint64_t,pair<int,int> >(element.m_pc, pair<int,int>(0,0) ));
		
		map<uint64_t, pair<int,int> > my_tab = map<uint64_t, pair<int,int> >();
	}

	if(element.isWrite())
		return ALLOCATE_IN_SRAM;
	else{
		//Test if the instruction is cold or not
		if(pc_counters[element.m_pc] < (PC_THRESHOLD-1))
			return ALLOCATE_IN_NVM; 
		else
			return ALLOCATE_IN_SRAM;
	}

}

void
InstructionPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{
	DPRINTF("InstructionPredictor::updatePolicy\n");
	
	CacheEntry* current;

	if(inNVM){
		current = m_tableNVM[set][index];
		m_replacementPolicyNVM_ptr->updatePolicy(set, index , 0);
	}		
	else{
		current = m_tableSRAM[set][index];
		m_replacementPolicySRAM_ptr->updatePolicy(set, index , 0);
	}
		
	current->policyInfo = m_cpt;
	if(element.isWrite()){
		if(current->saturation_counter < CACHE_THRESHOLD)
		{
			current->saturation_counter++;
			if(current->saturation_counter == CACHE_THRESHOLD)
			{
				//If the counter now saturates, update the inst info
				pc_counters[current->m_pc]++;
				if(pc_counters[current->m_pc] > PC_THRESHOLD){
					//cout << "Data set " << current->m_pc << " saturates " <<  cpt_time << endl;
					pc_counters[current->m_pc] = PC_THRESHOLD;
				}
				
			}
						
		}
		//We do nothing if the counter is already saturated			
		if(!m_isWarmup)
			stats_PCwrites[current->m_pc].first++;
	}
	else{ //Collect nb of R/W of datasets 

		if(!m_isWarmup)
			stats_PCwrites[current->m_pc].second++;
	
	} 	
	Predictor::updatePolicy(set, index, inNVM, element , isWBrequest);

	m_cpt++;
	DPRINTF("InstructionPredictor:: End updatePolicy\n");
}

void InstructionPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	DPRINTF("InstructionPredictor::insertionPolicy\n");

	if(inNVM){
		m_replacementPolicyNVM_ptr->insertionPolicy(set, index , 0);
		m_tableNVM[set][index]->saturation_counter = 0;		
	}
	else{
		m_replacementPolicySRAM_ptr->insertionPolicy(set, index , 0);
		m_tableSRAM[set][index]->saturation_counter = 0;
	}
	
	m_cpt++;
}

void
InstructionPredictor::printConfig(std::ostream& out){
	out << "\tSaturation Threshold:" << std::endl;
	out << "\t\t- CACHE_THRESHOLD : " << CACHE_THRESHOLD << std::endl;		
	out << "\t\t- PC_THRESHOLD : " << PC_THRESHOLD << std::endl;		
};


void 
InstructionPredictor::printStats(std::ostream& out)
{	
	int totalWrite = 0;
	for(auto p : stats_PCwrites)
	{
		pair<int,int> current = p.second;
		totalWrite += current.first;
	}
	
	out << "InstructionPredictor Results:" << endl;
	out << "PC Threshold : " << PC_THRESHOLD << endl;
	out << "Cache Line Threshold : " << CACHE_THRESHOLD << endl;
	
	out << "Total Write :" << totalWrite << endl;
	
	ofstream output_file;
	output_file.open(OUTPUT_PREDICTOR_FILE , std::ofstream::app);
	if(totalWrite > 0){
		output_file << "Write Intensity Per Instruction "<< endl;
		for(auto p : stats_PCwrites)
		{
			pair<int,int> current = p.second;
			output_file << p.first << "\t" << current.first << "\t" << current.second << endl;
		}
	}
	output_file.close();
	
	/*
	ofstream dataset_output_file(DATASET_OUTPUT_FILE);
	int i = 0;
	for(auto dataset = stats_datasets.begin() ; dataset != stats_datasets.end() ; dataset++ )
	{
		dataset_output_file << "DATASET\t" << i << "\tAddress\t0x"  << std::hex << dataset->first << endl;	
		int a = 0;
		map<uint64_t , pair<int,int> > current = dataset->second;
		
		for(auto cacheline = current.begin() ; cacheline != current.end() ; cacheline++ )
		{
			pair<int,int> current_cache_line = cacheline->second;
			dataset_output_file << "\t0x" << std::hex << cacheline->first << std::dec \
			<<  "\t" << current_cache_line.first << "\t" << current_cache_line.second << endl;
			a++;
		}
		i++;
	}
	dataset_output_file.close();*/
	
	Predictor::printStats(out);
}

int
InstructionPredictor::evictPolicy(int set, bool inNVM)
{	
	DPRINTF("InstructionPredictor::evictPolicy set %d\n" , set);
	int assoc_victim = -1;
	assert(m_replacementPolicyNVM_ptr != NULL);
	assert(m_replacementPolicySRAM_ptr != NULL);
	

	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);


	/* Update the status of the instruction */ 
	CacheEntry* current;
	if(inNVM)
		current = m_tableNVM[set][assoc_victim];
	else
		current = m_tableSRAM[set][assoc_victim];

	if(current->isValid){
		DPRINTF("InstructionPredictor::evictPolicy is Valid\n");
		if(current->saturation_counter < 2)
		{
			pc_counters[current->m_pc]--;
			if(pc_counters[current->m_pc] < 0)
				pc_counters[current->m_pc] = 0;
		}	
	}
	
	evictRecording(set , assoc_victim , inNVM);
	
	return assoc_victim;
}

