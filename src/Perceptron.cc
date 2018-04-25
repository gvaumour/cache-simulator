#include <vector>
#include <map>
#include <ostream>

#include "Perceptron.hh"

using namespace std;

PerceptronPredictor::PerceptronPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) :\
	Predictor(nbAssoc , nbSet , nbNVMways , SRAMtable , NVMtable , cache)
{	
	m_tableSize = simu_parameters.perceptron_table_size;

	m_features.clear();

	for(auto feature : simu_parameters.perceptron_features)
	{
		if(feature == "PC_LSB")
		{
			m_features.push_back( new FeatureTable(m_tableSize, feature));
			m_features_hash.push_back(hashingPC_LSB);		
		}
		else if(feature == "PC_MSB")
		{
			m_features.push_back( new FeatureTable(m_tableSize, feature));
			m_features_hash.push_back(hashingPC_MSB);				
		}
		else if(feature == "Addr_LSB")
		{
			m_features.push_back( new FeatureTable(m_tableSize, feature));
			m_features_hash.push_back(hashingAddr_LSB);						
		}
		else if(feature == "Addr_MSB")
		{
			m_features.push_back( new FeatureTable(m_tableSize, feature));
			m_features_hash.push_back(hashingAddr_MSB);	
		}
	}

	stats_nbBPrequests = vector<uint64_t>(1, 0);
	stats_nbDeadLine = vector<uint64_t>(1, 0);
}


PerceptronPredictor::~PerceptronPredictor()
{
	for(auto feature : m_features)
		delete feature;
}

allocDecision
PerceptronPredictor::allocateInNVM(uint64_t set, Access element)
{
//	if(element.isInstFetch())
//		return ALLOCATE_IN_NVM;

	int sum_prediction = 0;

	int actual_pc = m_cache->getActualPC();
	
	// All the set is a learning/sampled set independantly of its way or SRAM/NVM alloc
	bool isLearning = m_tableSRAM[set][0]->isLearning; 
	
	//Predict if we should bypass
	for(unsigned i = 0 ; i < m_features.size(); i++)
	{
		int hash = m_features_hash[i](element.m_address , element.m_pc , actual_pc);
		sum_prediction += m_features[i]->getBypassPrediction(hash);
	}

	if(isLearning)
	{
		return ALLOCATE_IN_SRAM;
	}
	else
	{	
		if(sum_prediction > simu_parameters.perceptron_threshold_bypass)
		{
			stats_nbBPrequests[stats_nbBPrequests.size()-1]++;
			return BYPASS_CACHE;
		}
		else
			return ALLOCATE_IN_SRAM;		
	}
	// Decide to allocate to SRAM or NVM 
	/*sum_prediction = 0;
	for(auto feature: m_features)
		sum_prediction += feature->getAllocationPrediction(set , element);

	if(sum_prediction >= 0)
		return ALLOCATE_IN_NVM;
	else 
		return ALLOCATE_IN_SRAM;
	*/
}

void
PerceptronPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest)
{

	CacheEntry *entry = get_entry(set , index , inNVM);
	if(entry->isLearning)
	{
		if( entry->perceptron_BPpred > -simu_parameters.perceptron_threshold_learning || !entry->predictedReused)
		{		
			uint64_t actual_pc = m_cache->getActualPC();
			for(unsigned i = 0 ; i < m_features.size() ; i++)
			{
				int hash = m_features_hash[i](entry->address , entry->m_pc , actual_pc);
				m_features[i]->decreaseConfidence(hash);
			}
		}
		setPrediction(entry);
	}
}

void
PerceptronPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	CacheEntry* current = get_entry(set , index , inNVM);

	if(current->isLearning)
		setPrediction(current);
	
	//updatePolicy(set , index, inNVM, element, false);
}

void 
PerceptronPredictor::setPrediction(CacheEntry* current)
{
	uint64_t actual_pc = m_cache->getActualPC();
	int bp_pred=0;
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = m_features_hash[i](current->address , current->m_pc , actual_pc);
		bp_pred += m_features[i]->getBypassPrediction(hash);
	}
	current->perceptron_BPpred = bp_pred;
	if(bp_pred > simu_parameters.perceptron_threshold_bypass)
		current->predictedReused = false;
	else
		current->predictedReused = true;	
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


	if(current->isValid && current->isLearning)
	{	
		uint64_t actual_pc = m_cache->getActualPC();

		if(current->predictedReused || current->perceptron_BPpred < simu_parameters.perceptron_threshold_learning)
		{
			for(unsigned i =  0 ; i < m_features.size() ; i++)
			{
				int hash = m_features_hash[i](current->address , current->m_pc , actual_pc);
				m_features[i]->increaseConfidence(hash);
			}
		}
		
		if(current->nbWrite == 0 && current->nbRead == 0)
			stats_nbDeadLine[stats_nbDeadLine.size()-1]++;
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

	ofstream file(FILE_OUTPUT_BYPASS);

	uint64_t total_BP = 0, total_DeadLines = 0;
	for(unsigned i = 0 ; i < stats_nbBPrequests.size() ; i++ )
	{
		total_BP+= stats_nbBPrequests[i];		
		total_DeadLines += stats_nbDeadLine[i];
		file << stats_nbBPrequests[i] << "\t" << stats_nbDeadLine[i] << endl;
	}
	file.close();
	
	out << entete << ":PerceptronPredictor:NBBypass\t" << total_BP << endl;
	out << entete << ":PerceptronPredictor:TotalDeadLines\t" << total_DeadLines << endl;
	

	Predictor::printStats(out, entete);
}



void PerceptronPredictor::printConfig(std::ostream& out, std::string entete) { 

	out << entete << ":Perceptron:TableSize\t" << m_tableSize << endl; 
	out << entete << ":Perceptron:BPThreshold\t" << simu_parameters.perceptron_threshold_bypass  << endl; 
	out << entete << ":Perceptron:NBFeatures\t" << simu_parameters.perceptron_features.size() << endl; 
	for(auto p : simu_parameters.perceptron_features)
		out << entete << ":PerceptronFeatures\t" << p << endl;

	Predictor::printConfig(out, entete);
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

void
PerceptronPredictor::openNewTimeFrame()
{
	stats_nbBPrequests.push_back(0);
	stats_nbDeadLine.push_back(0);
}

int hashingAddr_LSB(uint64_t addr , uint64_t missPC , uint64_t currentPC)
{
//	return bitSelect(addr , 0 , 7);

//	Addr << "cout " << std::hex << addr << " Current PC = " << currentPC << " : " << ( (missPC)^currentPC) % 256 << endl;
	return (addr^currentPC) % 256;
}

int hashingAddr_MSB(uint64_t addr , uint64_t missPC , uint64_t currentPC)
{
//	return bitSelect(addr , 20 , 27);
	return ( (addr>>7)^currentPC) % 256;
}

int
hashingPC_LSB(uint64_t addr , uint64_t missPC , uint64_t currentPC)
{
	return bitSelect(missPC , 0 , 7);
//	cout << "MissPC " << std::hex << missPC << " Current PC = " << currentPC << " : " << ( (missPC)^currentPC) % 256 << endl;
//	return ( (missPC)^currentPC) % 256;
}

int
hashingPC_MSB(uint64_t addr , uint64_t missPC , uint64_t currentPC)
{
//	return bitSelect(missPC , 40 , 47);
	return ( (missPC>>7)^currentPC) % 256;
}


