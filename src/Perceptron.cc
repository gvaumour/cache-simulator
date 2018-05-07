#include <cmath>
#include <vector>
#include <map>
#include <ostream>

#include "Perceptron.hh"

using namespace std;

ofstream dump_file(FILE_DUMP_PERCEPTRON);
int WATCHED_INDEX = 121;


PerceptronPredictor::PerceptronPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) :\
	Predictor(nbAssoc , nbSet , nbNVMways , SRAMtable , NVMtable , cache)
{	
	m_tableSize = simu_parameters.perceptron_table_size;
	m_features.clear();
	m_criterias_names = simu_parameters.perceptron_features;
	
	for(auto feature : simu_parameters.perceptron_features)
	{
		m_features.push_back( new FeatureTable(m_tableSize, feature));
		if(feature == "MissPC_LSB")
			m_features_hash.push_back(hashingMissPC_LSB);		
		else if(feature == "MissPC_LSB1")
			m_features_hash.push_back(hashingMissPC_LSB1);				
		else if(feature == "MissPC_MSB")
			m_features_hash.push_back(hashingMissPC_MSB);				

		else if(feature == "TagBlock")
			m_features_hash.push_back(hashingTag_block);
		else if(feature == "TagPage")
			m_features_hash.push_back(hashingTag_page);
	
		else if(feature == "MissCounter")
			m_features_hash.push_back(hashing_MissCounter);	
		else if(feature == "MissCounter1")
			m_features_hash.push_back(hashing_MissCounter1);
			
		else if(feature == "currentPC")
			m_features_hash.push_back(hashing_currentPC);
		else if(feature == "currentPC1")
			m_features_hash.push_back(hashing_currentPC1);
		else if(feature == "currentPC_3")
			m_features_hash.push_back(hashingcurrentPC_3);
		else if(feature == "currentPC_2")
			m_features_hash.push_back(hashingcurrentPC_2);
		else if(feature == "currentPC_1")
			m_features_hash.push_back(hashingcurrentPC_1);
		else
			assert(false && "Error while initializing the features , wrong name\n");

	}
	
	miss_counter = 0;
	m_cpt = 0;

	global_PChistory = deque<uint64_t>();

	stats_nbBPrequests = vector<uint64_t>(1, 0);
	stats_nbDeadLine = vector<uint64_t>(1, 0);
	stats_missCounter = vector<int>();
	/*
	stats_variances_buffer = vector<double>();
	stats_variances = vector<double>();
	*/
	stats_allocate = 0;
	stats_nbMiss = 0;
	stats_nbHits = 0;

	stats_update_learning = 0, stats_update = 0;
	stats_allocate = 0, stats_allocate_learning = 0;
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
	
	update_globalPChistory(element.m_pc);
	stats_nbMiss++;
	miss_counter++;
	if(miss_counter > 255)
		miss_counter = 255;
	
	// All the set is a learning/sampled set independantly of its way or SRAM/NVM alloc
	bool isLearning = m_tableSRAM[set][0]->isLearning; 
	
	if(isLearning)
	{
		return ALLOCATE_IN_SRAM;
	}
	else
	{	
		//Predict if we should bypass
		int sum_prediction = 0;
		//vector<double> samples;
		for(unsigned i = 0 ; i < m_features.size(); i++)
		{
			int hash = m_features_hash[i](element.m_address , element.m_pc , global_PChistory);
			int pred = m_features[i]->getBypassPrediction(hash);
			//samples.push_back(pred);
			sum_prediction += pred;
		}
		//Compute Variance of the sample
		/*double variance = 0, avg = (double)sum_prediction / (double) samples.size();
		for(unsigned i = 0 ; i < samples.size(); i++)
			variance += (samples[i] - avg) * (samples[i] - avg);

		variance = sqrt( variance / (double) samples.size());
		stats_variances_buffer.push_back(variance);
		*/
		
		if(sum_prediction > simu_parameters.perceptron_threshold_bypass)
		{
			stats_nbBPrequests[stats_nbBPrequests.size()-1]++;
			return BYPASS_CACHE;
		}
		else
		{
			return ALLOCATE_IN_SRAM;		
		}
	}
}

void
PerceptronPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest)
{
	update_globalPChistory(element.m_pc);
	
	stats_update++;
	
	CacheEntry *entry = get_entry(set , index , inNVM);
	entry->policyInfo = m_cpt++;
	if(entry->isLearning)
	{
		stats_update_learning++;
		for(unsigned i = 0 ; i < m_features.size() ; i++)
		{
			if( entry->perceptron_BPpred[i] > -simu_parameters.perceptron_threshold_learning || !entry->predictedReused[i])
			{
				int hash = m_features_hash[i](entry->address , entry->m_pc ,  global_PChistory);
				m_features[i]->decreaseConfidence(hash);
			}
		}
		setPrediction(entry);
	}
	
	stats_nbHits++;
	miss_counter--;
	if(miss_counter < 0)
		miss_counter = 0;


	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}

void
PerceptronPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* current = get_entry(set , index , inNVM);
	current->policyInfo = m_cpt++;

	stats_allocate++;

	if(current->isLearning)
	{
		stats_allocate_learning++;
		setPrediction(current);
	}
			
	Predictor::insertionPolicy(set , index , inNVM , element);
}


void
PerceptronPredictor::update_globalPChistory(uint64_t pc)
{
	
	global_PChistory.push_front(pc);
	if( global_PChistory.size() == 11)
		global_PChistory.pop_back();
}


void 
PerceptronPredictor::setPrediction(CacheEntry* current)
{
	int bp_pred=0;
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = m_features_hash[i](current->address , current->m_pc , global_PChistory);
		bp_pred = m_features[i]->getBypassPrediction(hash);
		current->perceptron_BPpred[i] = bp_pred;
		if(bp_pred > simu_parameters.perceptron_threshold_bypass)
			current->predictedReused[i] = false;
		else
			current->predictedReused[i] = true;
	}
	

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
		for(unsigned i =  0 ; i < m_features.size() ; i++)
		{
			if(current->predictedReused[i] || current->perceptron_BPpred[i] < simu_parameters.perceptron_threshold_learning)
			{
				int hash = m_features_hash[i](current->address , current->m_pc , global_PChistory);
 				m_features[i]->increaseConfidence(hash);
			}
		}
		
		if(current->nbWrite == 0 && current->nbRead == 0)
			stats_nbDeadLine[stats_nbDeadLine.size()-1]++;
	}
	
	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}

void
PerceptronPredictor::evictRecording( int id_set , int id_assoc , bool inNVM)
{ 
	Predictor::evictRecording(id_set, id_assoc, inNVM);
}


void 
PerceptronPredictor::printStats(std::ostream& out, std::string entete) { 

	uint64_t total_BP = 0, total_DeadLines = 0;
	for(unsigned i = 0 ; i < stats_nbBPrequests.size() ; i++ )
	{
		total_BP+= stats_nbBPrequests[i];		
		total_DeadLines += stats_nbDeadLine[i];
	}
	/*
	ofstream file(FILE_OUTPUT_PERCEPTRON);	
	for(unsigned i = 0; i < stats_variances.size() ; i++)
		file << stats_variances[i] << endl;
	file.close();*/
	
	out << entete << ":PerceptronPredictor:NBBypass\t" << total_BP << endl;
	out << entete << ":PerceptronPredictor:TotalDeadLines\t" << total_DeadLines << endl;
	out << entete << ":PerceptronPredictor:AllocationLearning\t" << stats_allocate_learning << endl;	
	out << entete << ":PerceptronPredictor:Allocation\t" << stats_allocate_learning + stats_allocate << endl;
	out << entete << ":PerceptronPredictor:UpdateLearning\t" << stats_update_learning << endl;
	out << entete << ":PerceptronPredictor:Update\t" << stats_update_learning + stats_update << endl;

	Predictor::printStats(out, entete);
}



void PerceptronPredictor::printConfig(std::ostream& out, std::string entete) { 

	out << entete << ":Perceptron:TableSize\t" << m_tableSize << endl; 
	out << entete << ":Perceptron:BPThreshold\t" << simu_parameters.perceptron_threshold_bypass  << endl;
	out << entete << ":Perceptron:LearningThreshold\t" << simu_parameters.perceptron_threshold_learning << endl; 
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
	
	stats_missCounter.push_back( miss_counter );
	stats_nbHits = 0, stats_nbMiss = 0;
	/*
	if(stats_variances_buffer.size() != 0)
	{
		double avg = 0;
		for(unsigned i = 0 ; i < stats_variances_buffer.size() ; i++)
			avg += stats_variances_buffer[i];
		avg /= (double)stats_variances_buffer.size();

		stats_variances.push_back(avg);	
		stats_variances_buffer.clear();
	}*/


	for(auto feature : m_features)
		feature->openNewTimeFrame();
	
	
	Predictor::openNewTimeFrame();
}

void 
PerceptronPredictor::finishSimu()
{
	
	for(auto feature : m_features)
		feature->finishSimu();

	dump_file.close();
	Predictor::finishSimu();
}


