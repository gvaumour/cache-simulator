#include <set>
#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "DBAMBPredictor.hh"

using namespace std;




ofstream dataset_file;
fstream firstAlloc_dataset_file;

int id_DATASET = 0;

/** DBAMBPredictor Implementation ***********/ 
DBAMBPredictor::DBAMBPredictor(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(id, nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {

	m_cpt = 1;
	m_RAP_assoc = simu_parameters.rap_assoc;
	m_RAP_sets = simu_parameters.rap_sets;

	m_DHPTable.clear();
	m_DHPTable.resize(m_RAP_sets);
	
	for(unsigned i = 0 ; i < m_RAP_sets ; i++)
	{
		m_DHPTable[i].resize(m_RAP_assoc);
		
		
		for(unsigned j = 0 ; j < m_RAP_assoc ; j++)
		{
			m_DHPTable[i][j] = new DHPEntry();
			m_DHPTable[i][j]->index = i;
			m_DHPTable[i][j]->assoc = j;
		}
	}
	/*
	if(simu_parameters.enableReuseErrorComputation)
		stats_history_SRAM.resize(nbSet);
	*/

	if(simu_parameters.DBAMB_signature.find("B") != std::string::npos)
	{	
		vector<string> split_line = split(simu_parameters.DBAMB_signature , 'B');
	
		if(split_line.size() != 0)
		{
			for(auto byte : split_line)
				m_hashingBytes.push_back(atoi(byte.c_str()));
			
			for(auto byte : m_hashingBytes)
				cout << "Hashing Function - Byte : " << byte << "B" << endl;
	
		}

	}

	
	//DPRINTF("RAPPredictor::Constructor m_DHPTable.size() = %lu , m_DHPTable[0].size() = %lu\n" , m_DHPTable.size() , m_DHPTable[0].size());

	m_rap_policy = new DBAMBLRUPolicy(m_RAP_assoc , m_RAP_sets , m_DHPTable);

	/* Stats init*/ 
	stats_nbSwitchDst.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors = vector< vector<int> >(NUM_RW_TYPE*NUM_RD_TYPE, vector<int>(NUM_RD_TYPE*NUM_RW_TYPE, 0));	
	

	if(simu_parameters.printDebug)
		dataset_file.open(RAP_TEST_OUTPUT_DATASETS);
	
	stats_nbMigrationsFromNVM.push_back(vector<int>(2,0));

	m_costParameters = simu_parameters.optTarget;
	
	if(simu_parameters.ratio_RWcost != -1)
		m_costParameters.costNVM[WRITE_ACCESS] = m_costParameters.costNVM[READ_ACCESS]*simu_parameters.ratio_RWcost;
	
	stats_error_wrongallocation = 0;
	stats_error_learning = 0;
	stats_error_wrongpolicy = 0;	

	stats_error_SRAMwrongallocation = 0;
	stats_error_SRAMlearning = 0;
	stats_error_SRAMwrongpolicy = 0;

	stats_allocate_NVM = 0, stats_allocate_SRAM = 0;
	stats_busyness_alloc_change = 0, stats_busyness_migrate_change = 0;
}


		
DBAMBPredictor::~DBAMBPredictor()
{
//	myFile.close();
	
	for(auto sets : m_DHPTable)
		for(auto entry : sets)
			delete entry;
	if(simu_parameters.printDebug)
		dataset_file.close();
}

allocDecision
DBAMBPredictor::allocateInNVM(uint64_t set, Access element)
{
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;
		
	uint64_t signature = hashingSignature(element);
	
	DHPEntry* rap_current = lookup(signature);
	if(rap_current  == NULL) // Miss in the DHP table, create the entry
	{
		if(!m_isWarmup)
			stats_RAP_miss++;	
		
		int index = indexFunction(signature);
		int assoc = m_rap_policy->evictPolicy(index);
		
		rap_current = m_DHPTable[index][assoc];
		
		rap_current->initEntry(element);
		rap_current->id = id_DATASET++;
		rap_current->signature = signature;
		dataset_file << "ID Dataset:" << rap_current->id << "\t" << signature << endl;
	}
	else
	{	
		if(!m_isWarmup)
			stats_RAP_hits++;
		rap_current->nbAccess++;
	}	
	
	m_rap_policy->updatePolicy(rap_current->index , rap_current->assoc);
	
	if(rap_current->des == BYPASS_CACHE)
	{
		rap_current->cptBypassLearning++;
		if(simu_parameters.printDebug)
		{
			dataset_file << "Dataset n°" << rap_current->id << ": BYPASS, Address: " << std::hex << \
			element.m_address << std::dec << "; Threshold=" << rap_current->cptBypassLearning << endl;		
		}
			
		if(rap_current->cptBypassLearning ==  simu_parameters.learningTH){
			rap_current->cptBypassLearning = 0;

			if(simu_parameters.printDebug)
			{
				dataset_file << "Dataset n°" << rap_current->id << ": Insert Learning Cl, " \
				<< std::hex << element.m_address << std::dec  << endl;			
			}
			return ALLOCATE_IN_NVM;		
		}
	}
	
	if(rap_current->des == ALLOCATE_PREEMPTIVELY)
	{
		if(element.isWrite())
			return ALLOCATE_IN_SRAM;		
		else
			return ALLOCATE_IN_NVM;
	}
	else
	{

		if(simu_parameters.enableDatasetSpilling)
		{
			if(rap_current->des == ALLOCATE_IN_NVM)
			{
				if( (m_isNVMbusy[set] && !m_isSRAMbusy[set]))
				{
					reportSpilling(rap_current , element.m_address , true , false);
					stats_busyness_alloc_change++;
					return ALLOCATE_IN_SRAM;
				}
				else
					return ALLOCATE_IN_NVM;			
			}
			else if(rap_current->des == ALLOCATE_IN_SRAM)
			{
				if(!m_isNVMbusy[set] && m_isSRAMbusy[set] && simu_parameters.enableDatasetSpilling)
				{
					stats_busyness_alloc_change++;
					reportSpilling(rap_current , element.m_address , true , true);
					return ALLOCATE_IN_NVM;
				}
				else
					return ALLOCATE_IN_SRAM;		
			}
			
		}
		return rap_current->des;
	}
}


void
DBAMBPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
		
	current->policyInfo = m_cpt;
	current->signature = hashingSignature(element);
		
	
	RD_TYPE reuse_class = RD_NOT_ACCURATE;
	DHPEntry* rap_current;

	if(inNVM)
		stats_allocate_NVM++;			
	else
		stats_allocate_SRAM++;			


	if( (rap_current= lookup(current->signature) ) != NULL )
	{

		/* Set the flag of learning cache line when bypassing */
		if(rap_current->des == BYPASS_CACHE)
		{
			current->isLearning = true;
		}
		else
		{
		
			if(element.isSRAMerror)
				reuse_class = RD_MEDIUM; 

			rap_current->rd_history.push_back(reuse_class); 
			rap_current->rw_history.push_back(element.isWrite());

			updateWindow(rap_current);		
		}
	
		reportAccess(rap_current, element, current, inNVM, "INSERTION",  str_RD_status[reuse_class]);
	}
	
	Predictor::insertionPolicy(set , index , inNVM , element);
	m_cpt++;
}


void
DBAMBPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{

	//DPRINTF("DBAMBPredictor::updatePolicy set %ld , index %ld\n" , set, index);

	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
	
	int rd = computeRd(set, index , inNVM);

	current->policyInfo = m_cpt;

	DHPEntry* rap_current = lookup(current->signature);

	if(rap_current)
	{

//			//DPRINTF("DBAMBPredictor::updatePolicy Reuse distance of the pc %lx, RD = %d\n", current->m_pc , rd );
		rap_current->rd_history.push_back(convertRD(rd));
		rap_current->rw_history.push_back(element.isWrite());		
		
		//A learning cl shows some reuse so we quit dead state 
		if(current->isLearning && rap_current->des == BYPASS_CACHE)
		{
			// Reset the window 
			RW_TYPE old_rw = rap_current->state_rw;
			RD_TYPE old_rd = rap_current->state_rd;
			
			rap_current->des = ALLOCATE_PREEMPTIVELY;
			rap_current->state_rd = RD_NOT_ACCURATE;
			rap_current->state_rw = RW_NOT_ACCURATE;
			rap_current->dead_counter = 0;

			if(!m_isWarmup)
				stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
			
			dataset_file << "Dataset n°" << rap_current->id << ": Reuse detected on a learning Cl, switching to learning state" << endl;
		}
		else
		{
			updateWindow(rap_current);
		
			reportAccess(rap_current , element, current, current->isNVM, string("UPDATE"), string(str_RD_status[convertRD(rd)]));
			dataset_file << "Position is " << rd << endl;
			if(simu_parameters.enableMigration )
				current = checkLazyMigration(rap_current , current , set , inNVM , index, element.isWrite());
		}
	}
	
	m_cpt++;
	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}


int
DBAMBPredictor::evictPolicy(int set, bool inNVM)
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
		
	//DPRINTF("DBAMBPredictor::evictPolicy set %d n assoc_victim %d \n" , set , assoc_victim);
	DHPEntry* rap_current = lookup(current->signature);
	//We didn't create an entry for this dataset, probably a good reason =) (instruction mainly) 
	if(rap_current != NULL)
	{
		if(rap_current->des != BYPASS_CACHE)
		{
			// A learning cache line on dead dataset goes here
			if( current->nbWrite == 0 && current->nbRead == 0)
				rap_current->dead_counter--;
			else
				rap_current->dead_counter++;
		
			if(rap_current->dead_counter > simu_parameters.deadSaturationCouter)
				rap_current->dead_counter = simu_parameters.deadSaturationCouter;
			else if(rap_current->dead_counter < -simu_parameters.deadSaturationCouter)
				rap_current->dead_counter = -simu_parameters.deadSaturationCouter;


			string a =  current->nbWrite == 0 && current->nbRead == 0 ? "DEAD" : "LIVELY";
			dataset_file << "Dataset n°" << rap_current->id << ": Eviction of a " << a << " CL, Address :" << \
				std::hex << current->address << std::dec << " DeadCounter=" << rap_current->dead_counter << endl;
				
			if(rap_current->dead_counter == -simu_parameters.deadSaturationCouter && simu_parameters.enableBP)
			{
				/* Switch the state of the dataset to dead */ 
				rap_current->des = BYPASS_CACHE;			
				dataset_file << "Dataset n°" << rap_current->id << ": Going into BYPASS mode " << endl;
				// Reset the window 
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;

				rap_current->state_rd = RD_NOT_ACCURATE;
				rap_current->state_rw = DEAD;
				rap_current->rd_history.clear();
				rap_current->rw_history.clear();
				rap_current->cptWindow = 0;	
				rap_current->dead_counter = 0;

				// Monitor state switching 
				if(!m_isWarmup)
					stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
			}
		}
	}
	
	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}


int
DBAMBPredictor::computeRd(uint64_t set, uint64_t  index , bool inNVM)
{
	vector<CacheEntry*> line;
	int ref_rd;
	
	if(inNVM){
		line = m_tableNVM[set];
		ref_rd = m_tableNVM[set][index]->policyInfo;
	}
	else{
		line = m_tableSRAM[set];
		ref_rd = m_tableSRAM[set][index]->policyInfo;
	}
	
	int position = 0;
	
	/* Determine the position of the cachel line in the LRU stack, the policyInfo is not enough */
	for(unsigned i = 0 ; i < line.size() ; i ++)
	{
		if(ref_rd < line[i]->policyInfo  && line[i]->isValid)
			position++;
	}
	return position;
}


void 
DBAMBPredictor::reportSpilling(DHPEntry* rap_current, uint64_t addr, bool isAlloc,  bool inNVM)
{
	string cl_location = inNVM ? "NVM" : "SRAM";
	string a = isAlloc ? "ALLOC" : "MIGRATION";
	
	dataset_file << "SPILLING:Dataset n°" << rap_current->id << ": DEFLECT " <<  a \
		<< ", Cl 0x" << std::hex << addr << std::dec <<  " cl alloc=" \
		<< cl_location << ", Dataset alloc=" << str_allocDecision[rap_current->des] << endl;
}

void
DBAMBPredictor::reportAccess(DHPEntry* rap_current, Access element, CacheEntry* current, bool inNVM, string entete, string reuse_class)
{
	string cl_location = inNVM ? "NVM" : "SRAM";
	string is_learning = current->isLearning ? "Learning" : "Regular";
	string access_type = element.isWrite() ? "WRITE" : "READ";
	
	string is_error= "";
	if(element.isSRAMerror)
		is_error = ", is an SRAM error";
	
	if(simu_parameters.printDebug)
	{
		dataset_file << entete << ":Dataset n°" << rap_current->id << ": [" \
		<< str_RW_status[rap_current->state_rw] << ","  << str_RD_status[rap_current->state_rd] \
		<< "] = " << str_allocDecision[rap_current->des] << "," \
		<< access_type << " on " << is_learning << " Cl 0x" << std::hex << current->address \
		<< std::dec << " allocated in " << cl_location<< ", Reuse=" << reuse_class \
		<< " , " << is_error << ", " << endl;	
	}
	
	
	if(inNVM && element.isWrite()){
		if(rap_current->des == ALLOCATE_IN_NVM)
			stats_error_wrongpolicy++;
		else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
			stats_error_learning++;
		else if(rap_current->des == ALLOCATE_IN_SRAM && inNVM)
			stats_error_wrongallocation++;	
	}
	if(element.isSRAMerror){
		if(rap_current->des == ALLOCATE_IN_SRAM)
			stats_error_SRAMwrongpolicy++;
		else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
			stats_error_SRAMlearning++;
		else if(rap_current->des == ALLOCATE_IN_NVM && !inNVM)
			stats_error_SRAMwrongallocation++;
	}
	if(simu_parameters.printDebug)
	{
		if(inNVM && element.isWrite())
		{
			dataset_file << "NVM Error : ";
			if(rap_current->des == ALLOCATE_IN_NVM)
				dataset_file << " Policy Error";		
			else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
				dataset_file << " Learning Error";
			else if(rap_current->des == ALLOCATE_IN_SRAM && inNVM)
				dataset_file << "Wrongly allocated cl";
			else
				dataset_file << " Des: " << str_allocDecision[rap_current->des] << "UNKNOW Error";
			dataset_file << endl;
		}
		
		if(element.isSRAMerror)
		{
			dataset_file << "SRAM Error : ";
		
			if(rap_current->des == ALLOCATE_IN_SRAM)
				dataset_file << "Wrong policy";
			else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
				dataset_file << " Learning Error";
			else if(rap_current->des == ALLOCATE_IN_NVM && !inNVM)
				dataset_file << "Wrongly allocated cl";
		
			dataset_file << endl;		
		}
	
	}
}

void
DBAMBPredictor::reportMigration(DHPEntry* rap_current, CacheEntry* current, bool fromNVM)
{
	string cl_location = fromNVM ? "from NVM" : "from SRAM";
	
	if(simu_parameters.printDebug)
	{
		dataset_file << "Migration:Dataset n°" << rap_current->id << ": [" << \
		str_RW_status[rap_current->state_rw] << ","  << str_RD_status[rap_current->state_rd] << "], Migration " << cl_location << \
		" Cl 0x" << std::hex << current->address << std::dec << " allocated in " << cl_location << endl;	
	}
	
}

CacheEntry*
DBAMBPredictor::checkLazyMigration(DHPEntry* rap_current , CacheEntry* current ,uint64_t set,bool inNVM, uint64_t index, bool isWrite)
{
	if( inNVM == false && rap_current->des == ALLOCATE_IN_NVM)
	{
		//cl is SRAM , check whether we should migrate to NVM 
		if(simu_parameters.enableDatasetSpilling && m_isNVMbusy[set])
		{
			reportSpilling(rap_current, current->address, false ,  inNVM);
			stats_busyness_migrate_change++; //If NVM tab is busy , don't migrate
		}
		else
		{
			//Trigger Migration
			int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache
			//DPRINTF("DBAMBPredictor:: Migration Triggered from SRAM, index %ld, id_assoc %d \n" , index, id_assoc);

			CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
					
			/** Migration incurs one read and one extra write */ 
			replaced_entry->nbWrite++;
			current->nbRead++;
		
			/* Record the write error migration */ 
			Predictor::migrationRecording();
		
			reportMigration(rap_current, current, false);
		
			m_cache->triggerMigration(set, index , id_assoc , false);
			if(!m_isWarmup)
				stats_nbMigrationsFromNVM.back()[FROMSRAM]++;


			current = replaced_entry;
		}

	}
	else if( rap_current->des == ALLOCATE_IN_SRAM && inNVM == true)
	{
		//cl is in NVM , check whether we should migrate to SRAM 
			
		//If SRAM tab is busy , don't migrate
		if(simu_parameters.enableDatasetSpilling && m_isSRAMbusy[set])
		{
			reportSpilling(rap_current, current->address, false ,  inNVM);
			stats_busyness_migrate_change++; //If NVM tab is busy , don't migrate
		}
		else 
		{		
			int id_assoc = evictPolicy(set, false);
		
			CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];

			/** Migration incurs one read and one extra write */ 
			replaced_entry->nbWrite++;
			current->nbRead++;
			reportMigration(rap_current, current, true);
	
			m_cache->triggerMigration(set, id_assoc , index , true);
		
			if(!m_isWarmup)
				stats_nbMigrationsFromNVM.back()[FROMNVM]++;

			current = replaced_entry;		
		}

	}
	return current;
}


void
DBAMBPredictor::updateWindow(DHPEntry* rap_current)
{

	if(rap_current->cptWindow < simu_parameters.window_size)
		rap_current->cptWindow++;
	else
	{
		RW_TYPE old_rw = rap_current->state_rw;
		RD_TYPE old_rd = rap_current->state_rd;
		allocDecision old_alloc = rap_current->des;
		
		determineStatus(rap_current);
		
		dataset_file << "Window:Dataset n°" << rap_current->id << ": NewWindow [" << str_RW_status[rap_current->state_rw] << "," \
		  	     << str_RD_status[rap_current->state_rd] << "]"  << endl;	
	

	
		if(!m_isWarmup)
			stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;

		if(old_rd != rap_current->state_rd || old_rw != rap_current->state_rw)
		{
			HistoEntry current = {rap_current->state_rw , rap_current->state_rd, rap_current->nbKeepState};

			if(!m_isWarmup)
				rap_current->stats_history.push_back(current);

			rap_current->nbSwitchState++;
			rap_current->nbKeepState = 0;					

			rap_current->des = convertState(rap_current);
			if(old_alloc == ALLOCATE_PREEMPTIVELY &&  simu_parameters.writeDatasetFile)
			{	
				firstAlloc_dataset_file << std::hex << "0x" << rap_current->signature << std::dec << "\t" << rap_current->des << endl;	
			}
		}
		else{
			rap_current->nbKeepState++;
			rap_current->nbKeepCurrentState++;			
		}

		/* Reset the window */			
		rap_current->rd_history.clear();
		rap_current->rw_history.clear();
		rap_current->cptWindow = 0;		
	}	
}



void 
DBAMBPredictor::finishSimu()
{
	//DPRINTF("RAPPredictor::FINISH SIMU\n");

	if(simu_parameters.readDatasetFile || simu_parameters.writeDatasetFile)	
		firstAlloc_dataset_file.close();
	
	if(m_isWarmup)
		return;
		
	for(auto lines : m_DHPTable)
	{
		for(auto entry : lines)
		{		
			if(entry->isValid)
			{
				HistoEntry current = {entry->state_rw , entry->state_rd , entry->nbKeepState};
				entry->stats_history.push_back(current);

				if(entry->nbKeepState + entry->nbSwitchState  > 0)
					stats_nbSwitchDst.push_back((double)entry->nbSwitchState / \
					  (double) (entry->nbKeepState + entry->nbSwitchState) );
			}
		}
	}
}


void 
DBAMBPredictor::determineStatus(DHPEntry* entry)
{
	if(entry->des == BYPASS_CACHE)
	{
		//All the info gathered here are from learning cache line
		//If we see any any reuse we learn again
		/*if(entry->rw_history.size() != 0)
		{
			entry->des = ALLOCATE_PREEMPTIVELY;
			entry->state_rd = RD_NOT_ACCURATE;
			entry->state_rw = RW_NOT_ACCURATE;
			entry->dead_counter = 0;
		}*/
		return;		
	}


	/* Count the number of read and write during window */
	vector<int> cpts_rw = vector<int>(2,0);
	for(auto isWrite : entry->rw_history)
	{
		if(isWrite)
			cpts_rw[INDEX_WRITE]++;
		else
			cpts_rw[INDEX_READ]++;
	}

	if(cpts_rw[INDEX_READ] == 0 && cpts_rw[INDEX_WRITE] > 0)
		entry->state_rw = WO;
	else if(cpts_rw[INDEX_READ] > 0 && cpts_rw[INDEX_WRITE] == 0)
		entry->state_rw = RO;
	else if(cpts_rw[INDEX_READ] > 0 && cpts_rw[INDEX_WRITE] > 0)
		entry->state_rw = RW;
	else
		entry->state_rw = DEAD;
		
	if(entry->state_rw == DEAD)
	{
		entry->state_rd = RD_NOT_ACCURATE;
		return;	
	}

	
	/* Determination of the rd type */
//	int window_rd = entry->rd_history.size();
//	vector<int> cpts_rd = vector<int>(NUM_RD_TYPE,0);
//	
//	for(auto rd : entry->rd_history)
//		cpts_rd[rd]++;
//		
//	int max = 0, index_max = -1;
//	for(unsigned i = 0 ; i < cpts_rd.size() ; i++)
//	{
//		if(cpts_rd[i] > max)
//		{
//			max = cpts_rd[i];
//			index_max = i;
//		}
//	}

	double costSRAM = 0;
	double costNVM =0;
	for(unsigned i = 0 ; i < entry->rd_history.size() ; i++)
	{
		bool isWrite = entry->rw_history[i];
		if(entry->rd_history[i] == RD_NOT_ACCURATE)
		{			
				costSRAM += m_costParameters.costDRAM[isWrite] + m_costParameters.costSRAM[isWrite]; 
				costNVM += m_costParameters.costDRAM[isWrite] + m_costParameters.costNVM[isWrite];		
		}
		else if(entry->rd_history[i] == RD_SHORT)
		{
			costSRAM += m_costParameters.costSRAM[isWrite]; 
			costNVM += m_costParameters.costNVM[isWrite];
		}
		else
		{
			costSRAM += m_costParameters.costDRAM[isWrite]; 
			costNVM += m_costParameters.costNVM[isWrite];	
		}
	}

	if(costSRAM > costNVM)
		entry->state_rd = (RD_TYPE) RD_MEDIUM;
	else
		entry->state_rd = (RD_TYPE) RD_SHORT;
			
	
//	if( ((double)max / (double)window_rd) > RAP_INACURACY_TH)
//		entry->state_rd = (RD_TYPE) index_max;
//	else
//		entry->state_rd = RD_NOT_ACCURATE;

	

			
//	/** Debugging purpose */	
//	if(entry->state_rd == RD_NOT_ACCURATE && entry->state_rw == DEAD)
//	{
//		cout <<"RD history\t";
//		for(auto p : entry->rd_history)
//			cout <<p << "\t";
//		cout <<endl;
//		cout <<"RW history\tNBWrite=" << cpts_rw[INDEX_WRITE] << "\tNBRead=" << cpts_rw[INDEX_READ] << endl;
//		cout <<"New state is " << str_RD_status[entry->state_rd] << "\t" << str_RW_status[entry->state_rw] << endl;	
//	}


	/*
	if( ((double)max / (double) RAP_WINDOW_SIZE) > RAP_INACURACY_TH)
		entry->state_rw = (RW_TYPE)index_max;
	else
		entry->state_rw = RW_NOT_ACCURATE;
	*/
}

allocDecision
DBAMBPredictor::convertState(DHPEntry* rap_current)
{
	RD_TYPE state_rd = rap_current->state_rd;
	RW_TYPE state_rw = rap_current->state_rw;

	if(state_rw == RO)
	{
		if(simu_parameters.sram_assoc <= simu_parameters.nvm_assoc)
		{
			return ALLOCATE_IN_NVM;	
		}
		else
		{
			if(state_rd == RD_SHORT || state_rd == RD_NOT_ACCURATE)
				return ALLOCATE_IN_NVM;
			else if(state_rd == RD_MEDIUM)
				return ALLOCATE_IN_SRAM;
			else 
				return BYPASS_CACHE;		
		}
		
	}
	else if(state_rw == RW || state_rw == WO)
	{
	
		if(simu_parameters.sram_assoc <= simu_parameters.nvm_assoc)
		{
			if(state_rd == RD_SHORT || state_rd == RD_NOT_ACCURATE)
				return ALLOCATE_IN_SRAM;
			else if(state_rd == RD_MEDIUM)
				return ALLOCATE_IN_NVM;
			else 
				return BYPASS_CACHE;		
		}
		else
		{
			return ALLOCATE_IN_SRAM;		
		}
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
DBAMBPredictor::printConfig(std::ostream& out, string entete1)
{
	string entete =entete1 + ":DBAMBPredictor:";
	
	out << entete << "DHPTableAssoc\t" << m_RAP_assoc << endl;
	out << entete << "DHPTableNBSets\t" << m_RAP_sets << endl;
	out << entete << "Windowsize\t" << simu_parameters.window_size << endl;
	out << entete << "InacurracyThreshold\t" << simu_parameters.rap_innacuracy_th << endl;

	string a =  simu_parameters.enableBP ? "TRUE" : "FALSE"; 
	out << entete << "BypassEnable\t" << a << endl;
	out << entete << "DEADCOUNTERSaturation\t" << simu_parameters.deadSaturationCouter << endl;
	out << entete << "LearningThreshold\t" << simu_parameters.learningTH << endl;
	a =  simu_parameters.enableMigration ? "TRUE" : "FALSE"; 
	out << entete << "MigrationEnable\t" << a << endl;
	a =  simu_parameters.enableDatasetSpilling ? "TRUE" : "FALSE"; 
	out << entete << "DatasetSpilling\t" << a << endl;
	if(simu_parameters.ratio_RWcost != -1)
		out << entete << "CostRWRatio\t" << simu_parameters.ratio_RWcost << endl;
	out << entete << "CostReadNVM\t" << m_costParameters.costNVM[READ_ACCESS] << endl;
	out << entete << "CostWriteNVM\t" << m_costParameters.costNVM[WRITE_ACCESS] << endl;
	out << entete << "HashingFunction\t" << simu_parameters.DBAMB_signature << endl;
	out << entete << "HashingBeginBit\t" << simu_parameters.DBAMB_begin_bit << endl;
	out << entete << "HashingEndBit\t" << simu_parameters.DBAMB_end_bit << endl;
	
	Predictor::printConfig(out, entete1);
}

void 
DBAMBPredictor::printStats(std::ostream& out, string entete)
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


	double migrationNVM=0, migrationSRAM = 0;

	if(simu_parameters.enableMigration)
	{

		ofstream file_migration_stats(RAP_MIGRATION_STATS);
		
		for(unsigned i = 0 ; i < stats_nbMigrationsFromNVM.size() ; i++)
		{
			migrationNVM += stats_nbMigrationsFromNVM[i][FROMNVM];
			migrationSRAM += stats_nbMigrationsFromNVM[i][FROMSRAM];			
			
			file_migration_stats << stats_nbMigrationsFromNVM[i][FROMNVM] << "\t" << stats_nbMigrationsFromNVM[i][FROMSRAM] << endl;	
		}
		file_migration_stats.close();
	}
	/*
	out << "\tRAP Table:" << endl;
	out << "\t\t NB Hits\t" << stats_RAP_hits << endl;
	out << "\t\t NB Miss\t" << stats_RAP_miss << endl;
	out << "\t\t Miss Ratio\t" << stats_RAP_miss*100.0/(double)(stats_RAP_hits+stats_RAP_miss)<< "%" << endl;
	*/
	
	/*out << "\tSource of Error, NVM error" << endl;
	out << "\t\tWrong Policy\t" << stats_error_wrongpolicy << endl;
	out << "\t\tWrong Allocation\t" << stats_error_wrongallocation << endl;
	out << "\t\tLearning\t" << stats_error_learning << endl;		
	*/
	/*
	if(simu_parameters.enableMigration)
		out << "\t\tMigration Error\t" << migrationSRAM << endl;		
	*/
	out << entete << ":DBAMBPredictor:NVM_medium_pred\t" << stats_NVM_medium_pred << endl;
	out << entete << ":DBAMBPredictor:NVM_medium_pred_errors\t" << stats_NVM_medium_pred_errors << endl;
	out << entete << ":DBAMBPredictor:BusynessMigrate\t" << stats_busyness_migrate_change << endl;
	out << entete << ":DBAMBPredictor:BusynessAlloc\t" << stats_busyness_alloc_change << endl;
	out << entete << ":DBAMBPredictor:AllocateSRAM\t" << stats_allocate_SRAM << endl;
	out << entete << ":DBAMBPredictor:AllocateNVM\t" << stats_allocate_NVM << endl;
	
//	out << "\tSource of Error, SRAM error" << endl;
//	out << "\t\tWrong Policy\t" << stats_error_SRAMwrongpolicy << endl;
//	out << "\t\tWrong Allocation\t" << stats_error_SRAMwrongallocation << endl;
//	out << "\t\tLearning\t" << stats_error_SRAMlearning << endl;		
	
	
	/*
	if(stats_nbSwitchDst.size() > 0)
	{
		double sum=0;
		for(auto d : stats_nbSwitchDst)
			sum+= d;
		out << "\t Average Switch\t" << sum / (double) stats_nbSwitchDst.size() << endl;	
	}*/
	


	double total = migrationNVM+migrationSRAM;

	if(total > 0){
		out << entete << ":DBAMBPredictor:NBMigration\t" << total << endl;
		out << entete << ":DBAMBPredictor:MigrationFromNVM\t" << migrationNVM << "\t" << migrationNVM*100/total << "%" << endl;
		out << entete << ":DBAMBPredictor:MigrationFromSRAM\t" << migrationSRAM << "\t" << migrationSRAM*100/total << "%" << endl;
	}
	
	Predictor::printStats(out, entete);
}

		

void 
DBAMBPredictor::openNewTimeFrame()
{ 

	if(m_isWarmup)
		return;

	stats_nbMigrationsFromNVM.push_back(vector<int>(2,0));
	dataset_file << "TimeFrame" << endl;
	Predictor::openNewTimeFrame();
}


DHPEntry*
DBAMBPredictor::lookup(uint64_t signature)
{
	uint64_t set = indexFunction(signature);
	for(unsigned i = 0 ; i < m_RAP_assoc ; i++)
	{
		if(m_DHPTable[set][i]->signature == signature)
			return m_DHPTable[set][i];
	}
	return NULL;
}


uint64_t
DBAMBPredictor::hashingFunction(Access element)
{	
	string total = buildHash( element.m_address , element.m_pc);
	string sig ="";
	for(unsigned i = 0 ; i < m_hashingBytes.size() ; i++)
	{
		assert(m_hashingBytes[i] < 16 && "Error on the hashing function bytes selection");
		sig += string(1, total[m_hashingBytes[i]]);
	}
	return hexToInt(sig);
}



uint64_t
DBAMBPredictor::hashingSignature(Access element)
{
	if(m_hashingBytes.size() != 0)
	{			
		return hashingFunction(element);		
	}
	else if(simu_parameters.DBAMB_signature == "first_pc")
	{
		return bitSelect( element.m_pc, simu_parameters.DBAMB_begin_bit , simu_parameters.DBAMB_end_bit);
	}
	else if(simu_parameters.DBAMB_signature == "addr")
	{
		return bitSelect(element.m_address , simu_parameters.DBAMB_begin_bit , simu_parameters.DBAMB_end_bit);
	}
	assert(true && "Wrong hashing signature parameter\n");
	return 0;
}


uint64_t 
DBAMBPredictor::indexFunction(uint64_t pc)
{
	return pc % m_RAP_sets;
}



RD_TYPE
DBAMBPredictor::evaluateRd(vector<int> reuse_distances)
{
	if(reuse_distances.size() == 0)
		return RD_NOT_ACCURATE;
	
	return convertRD(reuse_distances.back());
}

RD_TYPE
DBAMBPredictor::convertRD(int rd)
{

	if(simu_parameters.sram_assoc <= simu_parameters.nvm_assoc)
	{	
		if(rd < simu_parameters.sram_assoc)
			return RD_SHORT;
		else if(rd < simu_parameters.nvm_assoc)
			return RD_MEDIUM;
		else 
			return RD_NOT_ACCURATE;
	}
	else
	{
		if(rd < simu_parameters.nvm_assoc)
			return RD_SHORT;
		else if(rd < simu_parameters.sram_assoc)
			return RD_MEDIUM;
		else 
			return RD_NOT_ACCURATE;
	
	}
}

/************* Replacement Policy implementation for the test RAP table ********/ 

DBAMBLRUPolicy::DBAMBLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<DHPEntry*> >& rap_entries) :\
		DBAMBReplacementPolicy(nbAssoc , nbSet, rap_entries) 
{
	m_cpt = 1;
}

void
DBAMBLRUPolicy::updatePolicy(uint64_t set, uint64_t assoc)
{	

	
	m_rap_entries[set][assoc]->policyInfo = m_cpt;
	m_cpt++;

}

int
DBAMBLRUPolicy::evictPolicy(int set)
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




