#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "RAPPredictor.hh"

using namespace std;

static const char* str_RW_status[] = {"DEAD" , "WO", "RO" , "RW", "RW_NOTACC"};
static const char* str_RD_status[] = {"RD_SHORT" , "RD_MEDIUM", "RD_NOTACC", "UNKOWN"};
EnergyParameters energy_parameters;


/** RAPPredictor Implementation ***********/ 
RAPPredictor::RAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {

	m_RAPtable.clear();

	m_RAP_assoc = RAP_TABLE_ASSOC;
	m_RAP_sets = RAP_TABLE_SET;

	m_RAPtable.resize(m_RAP_sets);
	
	for(unsigned i = 0 ; i < m_RAP_sets ; i++)
	{
		m_RAPtable[i].resize(m_RAP_assoc);
		
		
		for(unsigned j = 0 ; j < m_RAP_assoc ; j++)
		{
			m_RAPtable[i][j] = new RAPEntry();
			m_RAPtable[i][j]->set = i;
			m_RAPtable[i][j]->assoc = j;
		}
	}
	
	DPRINTF("RAPPredictor::Constructor m_RAPtable.size() = %lu , m_RAPtable[0].size() = %lu\n" , m_RAPtable.size() , m_RAPtable[0].size());

	m_rap_policy = new RAPReplacementPolicy(m_RAP_assoc , m_RAP_sets , m_RAPtable);

	/* Stats init*/ 
	stats_nbSwitchDst.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors = vector< vector<int> >(NUM_RW_TYPE*NUM_RD_TYPE, vector<int>(NUM_RD_TYPE*NUM_RW_TYPE, 0));	
	
	/* Reseting the dataset_file */ 
	//ofstream dataset_file(RAP_TEST_OUTPUT_DATASETS);
	//dataset_file.close();


	stats_nbMigrationsFromNVM = vector<int>(2,0);
//	stats_switchDecision.clear();
//	stats_switchDecision.push_back(vector<vector<int>>(3 , vector<int>(3,0)));	
	
}


		
RAPPredictor::~RAPPredictor()
{
	for(auto sets : m_RAPtable)
		for(auto entry : sets)
			delete entry;
}

allocDecision
RAPPredictor::allocateInNVM(uint64_t set, Access element)
{

	DPRINTF("RAPPredictor::allocateInNVM set %ld\n" , set);
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;
		
	
	RAPEntry* rap_current = lookup(element.m_pc);
	if(rap_current  == NULL) // Miss in the RAP table
	{
		if(!m_isWarmup)
			stats_RAP_miss++;	
		

		DPRINTF("RAPPredictor::Installation of the entry PC=[%#lx] in the RAP table\n" , element.m_pc);
		int index = indexFunction(element.m_pc);
		int assoc = m_rap_policy->evictPolicy(index);
		
		rap_current = m_RAPtable[index][assoc];
		
		/* Dumping stats before erasing the RAP entry */ 
		if(rap_current->isValid)
		{
			DPRINTF("RAPPredictor::Eviction of the entry PC=[%#lx] of the RAP table\n" , rap_current->m_pc);					
			if(rap_current->nbKeepState > 0 && rap_current->nbSwitchState  > 0 && !m_isWarmup)
				stats_nbSwitchDst.push_back((double)rap_current->nbSwitchState / \
					 (double)(rap_current->nbKeepState+rap_current->nbSwitchState) );		
		}
		/******************/ 	

		rap_current->initEntry();
		rap_current->m_pc = element.m_pc;
	}
	else
	{	
		if(!m_isWarmup)
			stats_RAP_hits++;
		rap_current->nbAccess++;
	}	
	
	m_rap_policy->updatePolicy(rap_current->set , rap_current->assoc);
	
	
	
	
	if(rap_current->des == BYPASS_CACHE)
	{
		rap_current->cptBypassLearning++;
		
		
		if(rap_current->cptBypassLearning ==  RAP_LEARNING_THRESHOLD)
		{
			/* The history entry will be created when insertionPolicy is called */
			rap_current->cptBypassLearning = 0;
			return ALLOCATE_IN_NVM;		
		}
		else
		{	/*
			Window_entry entry;
			entry.isBypass = true;
			entry.isWrite = element.isWrite();
			updateWindow(rap_current, entry );*/
			return BYPASS_CACHE;
		}
	}
	else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
	{
		if(element.isWrite())
			return ALLOCATE_IN_SRAM;
		else
			return ALLOCATE_IN_NVM;
	}
	else
		return rap_current->des;
}

void 
RAPPredictor::finishSimu()
{
	DPRINTF("RAPPredictor::FINISH SIMU\n");


	if(m_isWarmup)
		return;
		
	for(auto lines : m_RAPtable)
	{
		for(auto entry : lines)
		{		
			if(entry->isValid)
			{
				HistoEntry current = {entry->state_rw , entry->state_rd , entry->nbKeepState};
				entry->stats_history.push_back(current);
//				dumpDataset(entry);

				if(entry->nbKeepState + entry->nbSwitchState  > 0)
					stats_nbSwitchDst.push_back((double)entry->nbSwitchState / \
					  (double) (entry->nbKeepState + entry->nbSwitchState) );
			}
		}
	}
}


RD_TYPE
RAPPredictor::computeRd(uint64_t set, uint64_t assoc , bool inNVM)
{
	vector<CacheEntry*> line;
	int ref_rd;
	int position = 0;
	
	if(inNVM){
		line = m_tableNVM[set];
		ref_rd = m_tableNVM[set][assoc]->policyInfo;
		for(unsigned i = 0 ; i < line.size() ; i ++){
			if(line[i]->policyInfo > ref_rd)
				position++;
		}
		
		return convertRD(position);
	}
	else{
		line = m_tableSRAM[set];
		ref_rd = m_tableSRAM[set][assoc]->policyInfo;
		for(unsigned i = 0 ; i < line.size() ; i ++){
			if(line[i]->policyInfo > ref_rd)
				position++;
		}
		

		return convertRD(position);
	}
}


void
RAPPredictor::dumpDataset(RAPEntry* entry)
{
	if(entry->stats_history.size() < 2)
		return;

	if(m_isWarmup)
		return;

	ofstream dataset_file(RAP_TEST_OUTPUT_DATASETS, std::ios::app);
	dataset_file << "Dataset\t0x" << std::hex << entry->m_pc << std::dec << endl;
	dataset_file << "\tNB Switch\t" << entry->nbSwitchState <<endl;
	dataset_file << "\tNB Keep State\t" << entry->nbKeepState <<endl;
	dataset_file << "\tHistory" << endl;
	for(auto p : entry->stats_history)
	{
		dataset_file << "\t\t" << str_RW_status[p.state_rw]<< "\t" << str_RD_status[p.state_rd] << "\t" << p.nbKeepState << endl;
	}
	dataset_file.close();
}


void
RAPPredictor::updatePolicy(uint64_t set,uint64_t assoc, bool inNVM, Access element, bool isWBrequest = false)
{

	DPRINTF("RAPPredictor::updatePolicy set %ld , assoc %ld\n" , set, assoc);

	CacheEntry* current = getEntry(set,assoc, inNVM);
	RD_TYPE rd = computeRd(set, assoc , inNVM);
	RAPEntry* rap_current = lookup(current->m_pc);
	
	if(rap_current)
	{	
		DPRINTF("RAPPredictor::updatePolicy Reuse distance of the pc %lx, RD = %d\n", current->m_pc , rd );
		Window_entry entry;
		entry.isWrite = element.isWrite();
		entry.rd = rd;
		updateWindow(rap_current , entry);
		
		if(rap_current->doMigration)
			checkMigration(rap_current , current , set , inNVM , assoc);

		m_rap_policy->updatePolicy(rap_current->set, rap_current->assoc);
		Predictor::updatePolicy(set , assoc , inNVM , element , isWBrequest);
	}
	
	DPRINTF("RAPPredictor::updatePolicy HERE\n");
}


void
RAPPredictor::checkMigration(RAPEntry* rap_current, CacheEntry* current, int set, bool inNVM, int assoc)
{
	if(rap_current->des == ALLOCATE_IN_NVM && inNVM == false)
	{
		//Trigger Migration
		int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache
		DPRINTF("RAPPredictor:: Migration Triggered from SRAM, SRAM_assoc=%ld, NVM_assoc=%d\n" , assoc, id_assoc);

		CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
					
		/** Migration incurs one read and one extra write */ 
		replaced_entry->nbWrite++;
		current->nbRead++;

		m_cache->triggerMigration(set, assoc , id_assoc , false);
		if(!m_isWarmup)
			stats_nbMigrationsFromNVM[inNVM]++;
	}
	else if( rap_current->des == ALLOCATE_IN_SRAM && inNVM == true)
	{
	
		DPRINTF("RAPPredictor:: Migration Triggered from NVM\n");

		int id_assoc = evictPolicy(set, false);
		
		CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];

		/** Migration incurs one read and one extra write */ 
		replaced_entry->nbWrite++;
		current->nbRead++;
	
		m_cache->triggerMigration(set, id_assoc , assoc , true);
		
		/* Record the write error migration */ 
		Predictor::migrationRecording();
		if(!m_isWarmup)
			stats_nbMigrationsFromNVM[inNVM]++;

	}
//	cout <<" FINISH Lazy Migration "<< endl;
}

void
RAPPredictor::insertionPolicy(uint64_t set, uint64_t assoc, bool inNVM, Access element)
{
	CacheEntry* current = getEntry(set, assoc , inNVM);
		
	RAPEntry* rap_current = lookup(current->m_pc);
	if( rap_current != NULL )
	{
	
		Window_entry entry;
		if( element.isSRAMerror)
			entry.rd = RD_MEDIUM;
		else 
			entry.rd = RD_NOT_ACCURATE;
		
		/* Set the flag of learning cache line when bypassing */
		if(rap_current->des == BYPASS_CACHE)
			current->isLearning = true;

		entry.isWrite = element.isWrite();
		updateWindow(rap_current, entry);	

		m_rap_policy->insertionPolicy(rap_current->set, rap_current->assoc);
	}
	
}


CacheEntry*
RAPPredictor::getEntry(int set, int assoc, bool inNVM)
{
	if(inNVM)
		return m_tableNVM[set][assoc];
	else
		return m_tableSRAM[set][assoc];
}

void 
RAPPredictor::printStats(std::ostream& out)
{	
	ofstream output_file(RAP_OUTPUT_FILE);
	
	for(int i = 0 ; i < NUM_RW_TYPE ;i++)
	{
		for(int j = 0 ; j < NUM_RD_TYPE ; j++)
			output_file << "\t[" << str_RW_status[i] << ","<< str_RD_status[j] << "]";
	}
	output_file << endl;
	
	for(unsigned i = 0 ; i < stats_ClassErrors.size() ; i++)
	{
		output_file << "[" << str_RW_status[i/NUM_RD_TYPE] << ","<< str_RD_status[i%NUM_RD_TYPE] << "]\t";
		for(unsigned j = 0 ; j < stats_ClassErrors[i].size() ; j++)
		{
			output_file << stats_ClassErrors[i][j] << "\t";
		}
		output_file << endl;
	}
	output_file.close();
	
	out << "\tRAP Table:" << endl;
	out << "\t\t NB Hits: " << stats_RAP_hits << endl;
	out << "\t\t NB Miss: " << stats_RAP_miss << endl;
	out << "\t\t Miss Ratio: " << stats_RAP_miss*100.0/(double)(stats_RAP_hits+stats_RAP_miss)<< "%" << endl;
	
	if(stats_nbSwitchDst.size() > 0)
	{
		double sum=0;
		for(auto d : stats_nbSwitchDst)
			sum+= d;
		out << "\t Average Switch\t" << sum / (double) stats_nbSwitchDst.size() << endl;	
	}
	
	if(simu_parameters.enableMigration)
	{
		double migrationNVM = (double) stats_nbMigrationsFromNVM[0];
		double migrationSRAM = (double) stats_nbMigrationsFromNVM[1];
		double total = migrationNVM+migrationSRAM;

		if(total > 0){
			out << "Predictor Migration:" << endl;
			out << "NB Migration :" << total << endl;
			out << "\t From NVM : " << migrationNVM*100/total << "%" << endl;
			out << "\t From SRAM : " << migrationSRAM*100/total << "%" << endl;	
		}

	
	}
	
	Predictor::printStats(out);
}

void
RAPPredictor::updateWindow(RAPEntry* rap_current, Window_entry entry)
{
	DPRINTF("RAPPredictor::updateWindow\n");
	rap_current->cptWindow++;
	rap_current->window.push_back(entry);	

	if(rap_current->cptWindow == simu_parameters.window_size){
		endWindows(rap_current);
	}
}

void
RAPPredictor::endWindows(RAPEntry* rap_current)
{
	DPRINTF("RAPPredictor::endWindows\n");
	RW_TYPE old_rw = rap_current->state_rw;
	RD_TYPE old_rd = rap_current->state_rd;

	determineStatus(rap_current);

	if(!m_isWarmup)
		stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;

	if(old_rd != rap_current->state_rd || old_rw != rap_current->state_rw)
	{
//		HistoEntry current = {rap_current->state_rw , rap_current->state_rd, rap_current->nbKeepState};

//		if(!m_isWarmup)
//			rap_current->stats_history.push_back(current);

		rap_current->nbSwitchState++;
		rap_current->nbKeepState = 0;					

//		rap_current->des = convertState(rap_current);
	}
	else{
		rap_current->nbKeepState++;
		rap_current->nbKeepCurrentState++;			
	}

	/* Reset the window */		
	rap_current->resetWindow();	
}

int
RAPPredictor::evictPolicy(int set, bool inNVM)
{	

		
	int assoc_victim = -1;
	assert(m_replacementPolicyNVM_ptr != NULL);
	assert(m_replacementPolicySRAM_ptr != NULL);

	CacheEntry* current = NULL;
	if(inNVM)
	{	
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
		current = m_tableNVM[set][assoc_victim];
	}
	else
	{
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);
		current = m_tableSRAM[set][assoc_victim];		
	}
		
	DPRINTF("RAPPredictor::evictPolicy set %d n assoc_victim %d \n" , set , assoc_victim);
	RAPEntry* rap_current = lookup(current->m_pc);
	//We didn't create an entry for this dataset, probably a good reason =) (instruction mainly) 
	if(rap_current != NULL)
	{
		if(rap_current->des != BYPASS_CACHE)
		{

			// A learning cache line on dead dataset goes here
			if(current->nbWrite == 0 && current->nbRead == 0)
				rap_current->dead_counter--;
			else
				rap_current->dead_counter++;
		
			if(rap_current->dead_counter > simu_parameters.deadSaturationCouter)
				rap_current->dead_counter = simu_parameters.deadSaturationCouter;
			else if(rap_current->dead_counter < -simu_parameters.deadSaturationCouter)
				rap_current->dead_counter = -simu_parameters.deadSaturationCouter;

			if(rap_current->dead_counter == -simu_parameters.deadSaturationCouter && simu_parameters.enableBP)
			{
				/* Switch the state of the dataset to dead */ 
				rap_current->des = BYPASS_CACHE;			

				// Reset the window 
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;

				rap_current->state_rd = RD_NOT_ACCURATE;
				rap_current->state_rw = DEAD;
			
				rap_current->resetWindow();
				rap_current->dead_counter = 0;

				// Monitor state switching 
				if(!m_isWarmup)
					stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
			}	
		}
		else
		{
			//On dead mode, if a learning cache line is evicted, goes back to living mode
			if(current->isLearning && (current->nbWrite > 0 || current->nbRead > 0))
			{
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;
			
				rap_current->initEntry();

				// Monitor state switching 
				if(!m_isWarmup)
					stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;

			}
		}
	
	}
	
	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}


void 
RAPPredictor::determineStatus(RAPEntry* entry)
{	
	double energySRAM = 0;
	double energyNVM = 0;

	int cptRead = 0, cptWrite = 0, cptBypass = 0;	
	int cptLong = 0, cptShort = 0, cptMedium = 0;
	for(auto a : entry->window)
	{
		cptRead += a.isWrite;
		cptWrite += a.isWrite;

		if(a.isBypass)
			cptBypass++;
		else
		{
			if(a.rd == RD_SHORT)
			{
				cptShort++;
				energySRAM += energy_parameters.costSRAM[a.isWrite]; 
				energyNVM += energy_parameters.costNVM[a.isWrite];
			}
			else if(a.rd == RD_MEDIUM)
			{
				cptMedium++;
				energySRAM += energy_parameters.costDRAM[a.isWrite]; 
				energyNVM += energy_parameters.costNVM[a.isWrite];	
			}
			else
			{
				cptLong++;
				energySRAM += energy_parameters.costDRAM[a.isWrite]; 
				energyNVM += energy_parameters.costDRAM[a.isWrite];
			}			
		}
	}

	/* Determine the state_rw and state_rd */ 
	if(cptRead == 0 && cptWrite != 0)
		entry->state_rw = RO;
	else if(cptRead > 0 && cptWrite == 0)
		entry->state_rw = WO;
	else
		entry->state_rw = RW;
	
	/***************************************/
	//if(entry->des == BYPASS_CACHE)
	//{
		/* If there are reuses on a dead datasets, we learn again */ 
	/*	if( (cptLong + cptBypass) != simu_parameters.window_size )
		{
			entry->des = ALLOCATE_PREEMPTIVELY;
			entry->state_rd = RD_NOT_ACCURATE;
			entry->state_rw = RW_NOT_ACCURATE;
		}
	}
	else
	{*/
		if(energySRAM > energyNVM)
			entry->des = ALLOCATE_IN_SRAM;
		else
			entry->des = ALLOCATE_IN_NVM; 
	//}
}

allocDecision
RAPPredictor::convertState(RAPEntry* rap_current)
{
	RD_TYPE state_rd = rap_current->state_rd;
	RW_TYPE state_rw = rap_current->state_rw;

	if(state_rw == RO)
	{
		return ALLOCATE_IN_NVM;	
	}
	else if(state_rw == RW || state_rw == WO)
	{
		if(state_rd == RD_SHORT || state_rd == RD_NOT_ACCURATE)
			return ALLOCATE_IN_SRAM;
		else if(state_rd == RD_MEDIUM)
			return ALLOCATE_IN_NVM;
		else 
			return BYPASS_CACHE;
	}
	else if(state_rw == DEAD)
	{
		// When in dead state, rd state is always innacurate, no infos 
		if(rap_current->des == ALLOCATE_IN_SRAM)
			return ALLOCATE_IN_NVM;
		else 
			return BYPASS_CACHE;	
	}
	else
	{ // State RW not accurate 
	
		if(state_rd == RD_SHORT)
			return ALLOCATE_IN_SRAM;
		else if( state_rd == RD_MEDIUM)
			return ALLOCATE_IN_NVM;
		else if(state_rd == RD_NOT_ACCURATE) // We don't know anything for sure, go preemptive
			return ALLOCATE_PREEMPTIVELY;
		else 
			return BYPASS_CACHE;
	}
}


void 
RAPPredictor::printConfig(std::ostream& out)
{
	out << "\t\tRAP Table Parameters" << endl;
	out << "\t\t\t- Assoc : " << m_RAP_assoc << endl;
	out << "\t\t\t- NB Sets : " << m_RAP_sets << endl;
	out << "\t\t Window size : " << simu_parameters.window_size << endl;
	out << "\t\t Inacurracy Threshold : " << simu_parameters.rap_innacuracy_th << endl;

	string a =  simu_parameters.enableBP ? "TRUE" : "FALSE"; 
	out << "\t\t Bypass Enable : " << a << endl;
	out << "\t\t Bypass DEAD COUNTER Saturation : " << simu_parameters.deadSaturationCouter << endl;
	out << "\t\t Bypass Learning Threshold : " << simu_parameters.learningTH << endl;
	a =  simu_parameters.enableMigration ? "TRUE" : "FALSE"; 
	out << "\t\t Lazy Migration Enable : " << a << endl;
}
		

void 
RAPPredictor::openNewTimeFrame()
{ 

	if(m_isWarmup)
		return;

//	stats_switchDecision.push_back(vector<vector<int> >(NUM_ALLOC_DECISION,vector<int>(NUM_ALLOC_DECISION,0)));

	Predictor::openNewTimeFrame();
}


RAPEntry*
RAPPredictor::lookup(uint64_t pc)
{
	uint64_t set = indexFunction(pc);
	for(unsigned i = 0 ; i < m_RAP_assoc ; i++)
	{
		if(m_RAPtable[set][i]->m_pc == pc)
			return m_RAPtable[set][i];
	}
	return NULL;
}

uint64_t 
RAPPredictor::indexFunction(uint64_t pc)
{
	return pc % m_RAP_sets;
}


RD_TYPE
RAPPredictor::convertRD(int rd)
{
	if(rd <= RAP_SRAM_ASSOC)
		return RD_SHORT;
	else if(rd <= RAP_NVM_ASSOC)
		return RD_MEDIUM;
	else 
		return RD_NOT_ACCURATE;
}

/************* Replacement Policy implementation for the test RAP table ********/ 

RAPReplacementPolicy::RAPReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> >& rap_entries) :\
		m_rap_entries(rap_entries), m_nb_set(nbSet) , m_assoc(nbAssoc)
{
	m_cpt = 1;
}

void
RAPReplacementPolicy::updatePolicy(uint64_t set, uint64_t assoc)
{	
	m_rap_entries[set][assoc]->policyInfo = m_cpt;
	m_cpt++;

}

int
RAPReplacementPolicy::evictPolicy(int set)
{
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(!m_rap_entries[set][i]->isValid)
			return i;
	}
	
	int smallest_time = m_rap_entries[set][0]->policyInfo , smallest_index = 0;
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(m_rap_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_rap_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}
	return smallest_index;
}





