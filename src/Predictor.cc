#include "Predictor.hh"
#include "ReplacementPolicy.hh"
#include "Cache.hh"

using namespace std;


Predictor::Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable , DataArray& NVMtable, HybridCache* cache) :\
	m_tableSRAM(SRAMtable), m_tableNVM(NVMtable)
{
	m_nb_set = nbSet;
	m_assoc = nbAssoc;
	m_nbNVMways = nbNVMways;
	m_nbSRAMways = nbAssoc - nbNVMways;
	
	m_replacementPolicyNVM_ptr = new LRUPolicy(m_nbNVMways , m_nb_set , NVMtable);
	m_replacementPolicySRAM_ptr = new LRUPolicy(m_nbSRAMways , m_nb_set, SRAMtable);	
	
	m_cache = cache;

	stats_NVM_errors = vector<int>(1, 0);
	stats_SRAM_errors = vector<int>(1, 0);
	stats_BP_errors = vector<int>(1,0);

	m_trackError = false;
	stats_MigrationErrors = 0;
	stats_COREerrors = 0;
	stats_WBerrors = 0;

	if(m_nbNVMways > m_nbSRAMways)
		m_trackError = true;
	
	if(m_trackError){
	
		/* Allocation of the SRAM missing tags array*/
		SRAM_missing_tags.resize(m_nbNVMways - m_nbSRAMways);
		for(int i = 0 ; i < m_nbNVMways - m_nbSRAMways ; i++)
		{
			SRAM_missing_tags[i].resize(m_nb_set);
			for(int j = 0 ; j < m_nb_set ; j++)
			{
				SRAM_missing_tags[i][j] = new MissingTagEntry();
			}
		}

		/* Allocation of the BP missing tags array*/
		BP_missing_tags.resize(m_nb_set);
		for(int i = 0 ; i < m_nb_set; i++)
		{
			BP_missing_tags[i].resize(m_assoc);
			for(int j = 0 ; j < m_nb_set ; j++)
			{
				BP_missing_tags[i][j] = new MissingTagEntry();
			}
		}

	}
}

Predictor::~Predictor()
{
	if(m_trackError){
		for(unsigned i = 0 ; i < SRAM_missing_tags.size() ; i++)
		{
			for(unsigned j = 0 ; j < SRAM_missing_tags[i].size() ; j++)
			{
				delete SRAM_missing_tags[i][j];
			}
		}
		for(unsigned i = 0 ; i < BP_missing_tags.size() ; i++)
		{
			for(unsigned j = 0 ; j < BP_missing_tags[i].size() ; j++)
			{
				delete BP_missing_tags[i][j];
			}
		}
	}
	delete m_replacementPolicyNVM_ptr;
	delete m_replacementPolicySRAM_ptr;
}

void
Predictor::startWarmup()
{
	m_isWarmup = true;
}

void
Predictor::stopWarmup()
{
	m_isWarmup = false;
}


void  
Predictor::evictRecording(int set, int assoc_victim , bool inNVM)
{

	if(inNVM || !m_trackError)
		return;
		
	CacheEntry* current = m_tableSRAM[set][assoc_victim];
	
	//Choose the oldest victim for the missing tags to replace from the current one 	
	uint64_t cpt_longestime = SRAM_missing_tags[0][set]->last_time_touched;
	uint64_t index_victim = 0;
	
	for(unsigned i = 0 ; i < SRAM_missing_tags.size(); i++)
	{
		if(!SRAM_missing_tags[i][set]->isValid)
		{
			index_victim = i;
			break;
		}	
		
		if(cpt_longestime > SRAM_missing_tags[i][set]->last_time_touched){
			cpt_longestime = SRAM_missing_tags[i][set]->last_time_touched; 
			index_victim = i;
		}	
	}

	SRAM_missing_tags[index_victim][set]->addr = current->address;
	SRAM_missing_tags[index_victim][set]->last_time_touched = cpt_time;
	SRAM_missing_tags[index_victim][set]->isValid = current->isValid;


}


void
Predictor::insertRecord(int set, int assoc, bool inNVM)
{
	//Update the missing tag as a new cache line is brough into the cache 
	//Has to remove the MT entry if it exists there 
	if(inNVM || !m_trackError)
	{
	
		CacheEntry* current = m_tableSRAM[set][assoc];
		for(unsigned i = 0 ; i < SRAM_missing_tags.size(); i++)
		{
			if(SRAM_missing_tags[i][set]->addr == current->address)
			{
				SRAM_missing_tags[i][set]->isValid = false;
			}
		}
	}	
}

bool
Predictor::recordAllocationDecision(uint64_t set, Access element, allocDecision des)
{	
	if(!m_trackError)
		return false;
	
	bool find = false , isBPerror = false;
	DPRINTF("Predictor::recordAllocationDecision Set %d, element.m_address = 0x%lx, des = %s \n ", set, element.m_address, allocDecision_str[des]);

	//Look if the element already exists , if yes update it, if no insert it
	for(unsigned i = 0 ; i < BP_missing_tags[set].size() ; i++)
	{	
		if(BP_missing_tags[set][i]->addr == element.m_address)
		{
			find = true;
			BP_missing_tags[set][i]->last_time_touched = cpt_time;
			DPRINTF("Predictor::recordAllocationDecision Entry Already present, Update time\n");

			
			//Bypass an access that would be a hit , BP ERROR ! 
			if(des == BYPASS_CACHE && BP_missing_tags[set][i]->isBypassed){
			
				DPRINTF("Predictor::recordAllocationDecision BP_error detected\n");
				BP_missing_tags[set][i]->isBypassed = (des == BYPASS_CACHE);
				isBPerror = false;
				
				if(!m_isWarmup)		
					stats_BP_errors[stats_BP_errors.size()-1]++;
			}
	
			break;
		}
	}
	if(!find)
	{
		DPRINTF("Predictor::recordAllocationDecision Didn't find the entry in the BP Missing Tags\n");
		int index_oldest =0;
		uint64_t oldest = BP_missing_tags[set][0]->last_time_touched;
		for(unsigned i = 0 ; i < BP_missing_tags[set].size() ; i++)
		{
			if(!BP_missing_tags[set][i]->isValid)		
			{
				index_oldest= i;
				break;
			}
		
			if(BP_missing_tags[set][i]->last_time_touched < oldest)
			{
				index_oldest = i;
				oldest = BP_missing_tags[set][i]->last_time_touched;
			}
			
		}
		
		if(BP_missing_tags[set][index_oldest]->isValid)
		{
			DPRINTF("Predictor::Eviction of entry 0x%lx\n",BP_missing_tags[set][index_oldest]->addr);
		}
		DPRINTF("Predictor::Insertion of the new entry at assoc %d\n", index_oldest);
		
		BP_missing_tags[set][index_oldest]->last_time_touched = cpt_time;
		BP_missing_tags[set][index_oldest]->addr = element.m_address;
		BP_missing_tags[set][index_oldest]->isValid = true;		
		BP_missing_tags[set][index_oldest]->isBypassed = (des == BYPASS_CACHE);
	}
	return isBPerror;
}


bool
Predictor::checkSRAMerror(uint64_t block_addr , int id_set)
{

	if(m_trackError || m_isWarmup){
		//An error in the SRAM part is a block that miss in the SRAM array 
		//but not in the MT array
		for(unsigned i = 0 ; i < SRAM_missing_tags.size(); i++)
		{
			if(SRAM_missing_tags[i][id_set]->addr == block_addr && SRAM_missing_tags[i][id_set]->isValid)
			{
				//DPRINTF("BasePredictor::checkMissingTags Found SRAM error as %#lx is present in MT tag %i \n", block_addr ,i);  
				stats_SRAM_errors[stats_SRAM_errors.size()-1]++;
				return true;
			}
		}	
	}
	return false;
}


void
Predictor::openNewTimeFrame()
{
	if(m_isWarmup)
		return;
		
	DPRINTF("BasePredictor::openNewTimeFrame New Time Frame Start\n");  
	stats_NVM_errors.push_back(0);
	stats_SRAM_errors.push_back(0);
	stats_BP_errors.push_back(0);
	stats_nbLLCaccessPerFrame = 0;
}

void
Predictor::migrationRecording()
{

	if(!m_isWarmup)
	{
		stats_NVM_errors[stats_NVM_errors.size()-1]++;
		stats_MigrationErrors++;	
	}
}


void
Predictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{	
	if(m_isWarmup)
		return;


	stats_nbLLCaccessPerFrame++;
	//An error in the NVM side is when handling a write 
	if(inNVM && element.isWrite()){
		stats_NVM_errors[stats_NVM_errors.size()-1]++;

		if(isWBrequest)
			stats_WBerrors++;
		else
			stats_COREerrors++;
	}
}
				 
void 
Predictor::printStats(std::ostream& out)
{

	uint64_t totalNVMerrors = 0, totalSRAMerrors= 0, totalBPerrors = 0;	
	ofstream output_file;
	output_file.open(PREDICTOR_OUTPUT_FILE);
	for(unsigned i = 0 ; i <  stats_NVM_errors.size(); i++)
	{	
		output_file << stats_NVM_errors[i] << "\t" << stats_SRAM_errors[i] << "\t" << stats_BP_errors[i] << endl;	
		totalNVMerrors += stats_NVM_errors[i];
		totalSRAMerrors += stats_SRAM_errors[i];
		totalBPerrors += stats_BP_errors[i];
	}
	output_file.close();

	out << "\tPredictor Errors:" <<endl;
	out << "\t\tNVM Error\t" << totalNVMerrors << endl;
	out << "\t\t\t-From WB\t"  << stats_WBerrors << endl;
	out << "\t\t\t-From Core\t" <<  stats_COREerrors << endl;
	out << "\t\t\t-From Migration\t" <<  stats_MigrationErrors << endl;
	out << "\t\tSRAM Error\t" << totalSRAMerrors << endl;
	out << "\t\tBP Error\t" << totalBPerrors << endl;
}


LRUPredictor::LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache)\
 : Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache)
{
	/* With LRU policy, the cache is not hybrid, only NVM or only SRAM */ 
//	assert(m_nbNVMways == 0 || m_nbSRAMways == 0);
	
	m_cpt = 1;
}


allocDecision
LRUPredictor::allocateInNVM(uint64_t set, Access element)
{
	// Always allocate on the same data array 
	
	if(m_nbNVMways == 0)
		return ALLOCATE_IN_SRAM;
	else 
		return ALLOCATE_IN_NVM;
}

void
LRUPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{
	if(inNVM)
		m_replacementPolicyNVM_ptr->updatePolicy(set, index , 0);
	else
		m_replacementPolicySRAM_ptr->updatePolicy(set, index , 0);

	Predictor::updatePolicy(set , index , inNVM , element);
	m_cpt++;
}

void
LRUPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{	
	if(inNVM)
		m_replacementPolicyNVM_ptr->insertionPolicy(set, index , 0);
	else
		m_replacementPolicySRAM_ptr->insertionPolicy(set, index , 0);
	
	Predictor::insertRecord(set, index , inNVM);
	m_cpt++;
}

int
LRUPredictor::evictPolicy(int set, bool inNVM)
{
	int assoc_victim;
	
	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim =  m_replacementPolicySRAM_ptr->evictPolicy(set);
	
	evictRecording(set , assoc_victim , inNVM);
	return assoc_victim;
}


allocDecision
PreemptivePredictor::allocateInNVM(uint64_t set, Access element)
{

	if( m_nbSRAMways == 0 )
		return ALLOCATE_IN_NVM;
	else if(m_nbNVMways == 0)
		return ALLOCATE_IN_SRAM;


	if(element.isWrite())
		return ALLOCATE_IN_SRAM;
	else
		return ALLOCATE_IN_NVM;
}


/********************************************/ 
