#include <set>
#include <fstream>
#include <vector>
#include <map>
#include <ostream>
#include <math.h>

#include "Perceptron.hh"

using namespace std;

void 
CriteriaEntry::updateDatasetState()
{
	
	CostFunctionParameters params = simu_parameters.optTarget;
	
	double E_SRAM = (params.costDRAM[WRITE_ACCESS] + params.costSRAM[WRITE_ACCESS]) * \
		       		 (cpts[WRITE_ACCESS][RD_NOT_ACCURATE] + cpts[WRITE_ACCESS][RD_MEDIUM]) + \
		        (params.costDRAM[READ_ACCESS] + params.costSRAM[WRITE_ACCESS]) * \
		       		 (cpts[READ_ACCESS][RD_NOT_ACCURATE] + cpts[READ_ACCESS][RD_MEDIUM]) + \
		       params.costSRAM[READ_ACCESS] * cpts[READ_ACCESS][RD_SHORT] + \
		       params.costSRAM[WRITE_ACCESS] * cpts[WRITE_ACCESS][RD_SHORT];	

	double E_NVM = (params.costDRAM[WRITE_ACCESS] + params.costNVM[WRITE_ACCESS]) * \
	       		cpts[WRITE_ACCESS][RD_NOT_ACCURATE] + \
		        (params.costDRAM[READ_ACCESS] + params.costNVM[WRITE_ACCESS]) * \
		       		 cpts[READ_ACCESS][RD_NOT_ACCURATE] + \
		       params.costNVM[READ_ACCESS] *  ( cpts[READ_ACCESS][RD_MEDIUM] + cpts[READ_ACCESS][RD_SHORT]) + \
		       params.costNVM[WRITE_ACCESS] * ( cpts[WRITE_ACCESS][RD_MEDIUM] + cpts[WRITE_ACCESS][RD_SHORT]);

	if(E_SRAM > E_NVM)
		des = ALLOCATE_IN_SRAM;
	else
		des = ALLOCATE_IN_NVM;
		
		
	cpts = vector< vector<int> >(NUM_RW_TYPE, vector<int>(NUM_RD_TYPE, 0));
	cptWindow = 0;
}



CriteriaTable::CriteriaTable( int size, hashing_function hash , string name)
{
	m_size = size;
	m_hash = hash;
	m_name = name;
	
	for(int i = 0 ; i < m_size; i++)
		m_table[i] = new CriteriaEntry();
}

CriteriaTable::~CriteriaTable()
{
	for(auto entry: m_table)
		delete entry;		
}

CriteriaEntry*
CriteriaTable::lookup(CacheEntry* entry)
{
	int index = m_hash(entry);
	assert(index < m_size && "Error during Index Function");
	
	return m_table[index];
}

void 
CriteriaTable::updateEntry(CacheEntry* entry, Access element, RD_TYPE rd)
{
	CriteriaEntry* crit_entry = lookup(entry);
	
	if(!crit_entry->isValid)
	{	
		//Init the crit entry
		crit_entry->cpts = vector< vector<int> >(NUM_RW_TYPE, vector<int>(NUM_RD_TYPE, 0));
		crit_entry->cptWindow = 0;
		crit_entry->isValid = true;
	}

	if( element.isWrite() ) 
		crit_entry->cpts[rd][WRITE_ACCESS]++;
	else
		crit_entry->cpts[rd][READ_ACCESS]++;
	
	crit_entry->cptWindow++;

	if(crit_entry->cptWindow > simu_parameters.perceptron_windowSize)
		crit_entry->updateDatasetState();	

}


int
CriteriaTable::getAllocationPrediction(int set , Access element)
{
	CacheEntry dummy;
	dummy.m_pc = element.m_pc;
	dummy.address = element.m_address;
	CriteriaEntry* entry = lookup(&dummy);
	
	if(!entry)
		return 0;
		
	if(entry->des == ALLOCATE_IN_NVM)
		return entry->allocation_counter;
	else if( entry->des == ALLOCATE_IN_SRAM)
		return (-1) * entry->allocation_counter;
	else
		return 0;
}

PerceptronPredictor::PerceptronPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) :\
	Predictor(nbAssoc , nbSet , nbNVMways , SRAMtable , NVMtable , cache)
{	
	m_tableSize = simu_parameters.criteriaTable_size;

	for(auto crit : simu_parameters.perceptron_criterias)
	{
		if(crit == "PC_LSB")
			m_criterias.push_back( new CriteriaTable(m_tableSize, hashingPC_LSB , crit));
		else if(crit == "PC_MSB")
			m_criterias.push_back( new CriteriaTable(m_tableSize, hashingPC_MSB , crit));
		else if(crit == "Addr_LSB")
			m_criterias.push_back( new CriteriaTable(m_tableSize, hashingAddr_LSB , crit));
		else if(crit == "Addr_MSB")
			m_criterias.push_back( new CriteriaTable(m_tableSize, hashingAddr_MSB , crit));
	}

}


PerceptronPredictor::~PerceptronPredictor()
{
	for(auto crit : m_criterias)
		delete crit;
}

allocDecision
PerceptronPredictor::allocateInNVM(uint64_t set, Access element)
{
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;

	int sum_prediction = 0;
	for(auto crit: m_criterias)
	{
		sum_prediction += crit->getAllocationPrediction(set , element);
	}

	if(sum_prediction >= 0)
		return ALLOCATE_IN_NVM;
	else 
		return ALLOCATE_IN_SRAM;
}

void
PerceptronPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest)
{
	CacheEntry *entry = get_entry(set , index , inNVM);

	RD_TYPE rd = classifyRD(set , index , inNVM);
	
	for(auto crit: m_criterias)
		crit->updateEntry(entry , element , rd );


	//Should we migrate ? 
}

void
PerceptronPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry *entry = get_entry(set , index , inNVM);

	uint64_t block_addr = bitRemove(element.m_address , 0 , log2(BLOCK_SIZE));
	RD_TYPE rd = Predictor::hitInMissingTags(block_addr, set) ? RD_MEDIUM : RD_NOT_ACCURATE;
	
	for(auto crit: m_criterias)
		crit->updateEntry(entry , element , rd );
}

int PerceptronPredictor::evictPolicy(int set, bool inNVM)
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


	return assoc_victim;
}

void
PerceptronPredictor::evictRecording( int id_set , int id_assoc , bool inNVM)
{ 
	Predictor::evictRecording(id_set, id_assoc, inNVM);
}


void 
PerceptronPredictor::printStats(std::ostream& out, std::string entete) { 
	Predictor::printStats(out, entete);
}



void PerceptronPredictor::printConfig(std::ostream& out, std::string entete) { 

	cout << entete << ":Perceptron:TableSize\t" << m_tableSize << endl; 
	Predictor::printConfig(out, entete);
	for(auto p : simu_parameters.perceptron_criterias)
		cout << entete << ":PerceptronCriteria\t" << p << endl;
}

CacheEntry*
PerceptronPredictor::get_entry(uint64_t set , uint64_t index , bool inNVM)
{
	if(inNVM)
		return m_tableNVM[set][index];
	else
		return m_tableSRAM[set][index];
}

RD_TYPE 
PerceptronPredictor::classifyRD(int set , int index , bool inNVM)
{
	if(!inNVM)
		return RD_SHORT;
	
	vector<CacheEntry*> line = m_tableNVM[set];
	int ref_rd = m_tableNVM[set][index]->policyInfo;
	int position = 0;
	
	/* Determine the position of the cachel line in the LRU stack, the policyInfo is not enough */
	for(unsigned i = 0 ; i < line.size() ; i ++)
	{
		if(line[i]->policyInfo > ref_rd)
			position++;
	}
	
	if( position < 4 )
		return RD_SHORT;
	else
		return RD_MEDIUM;
}

int hashingAddr_LSB(CacheEntry* entry)
{
	return bitSelect(entry->address , 0 , 7);
}

int hashingAddr_MSB(CacheEntry* entry)
{
	return bitSelect(entry->address , 40 , 48);
}

int
hashingPC_LSB(CacheEntry* entry)
{
	return bitSelect(entry->m_pc , 0 , 7);
}

int
hashingPC_MSB(CacheEntry* entry)
{
	return bitSelect(entry->m_pc , 40 , 48);
}


