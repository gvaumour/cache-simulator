#include <cmath>
#include <vector>
#include <map>
#include <ostream>

#include "Perceptron.hh"

using namespace std;

int WATCHED_INDEX = 121;

std::deque<uint64_t> PerceptronPredictor::m_global_PChistory;
std::deque<uint64_t> PerceptronPredictor::m_callee_PChistory;


PerceptronPredictor::PerceptronPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) :\
	Predictor(nbAssoc , nbSet , nbNVMways , SRAMtable , NVMtable , cache)
{	
	m_tableSize = simu_parameters.perceptron_table_size;

	m_BP_features.clear();
	m_Allocation_features.clear();

	m_BP_criterias_names = simu_parameters.perceptron_BP_features;
	m_Allocation_criterias_names = simu_parameters.perceptron_Allocation_features;

	m_need_callee_file = false;
	m_enableBP = simu_parameters.perceptron_enableBypass;
	
	m_enableAllocation = true;
	if(simu_parameters.nvm_assoc == 0 || simu_parameters.sram_assoc == 0)
		m_enableAllocation = false;
	
	
	cout <<  "\tBP Features used : ";
	for(auto p : simu_parameters.perceptron_BP_features)
		cout << p << ",";
	cout << endl;
	cout <<  "\tAlloc Features used : ";
	for(auto p : simu_parameters.perceptron_Allocation_features)
		cout << p << ",";
	cout << endl;

	
	if(m_enableBP)
		initFeatures(true);
	initFeatures(false);
	
	if(m_need_callee_file)
	{
		m_callee_map = map<uint64_t,int>();
		load_callee_file();
	}
		
	
	miss_counter = 0;
	m_cpt = 0;

	m_global_PChistory = deque<uint64_t>();
	m_callee_PChistory = deque<uint64_t>();

	stats_nbBPrequests = vector<uint64_t>(1, 0);
	stats_nbDeadLine = vector<uint64_t>(1, 0);
	stats_missCounter = vector<int>();
	
	if(simu_parameters.perceptron_compute_variance)
	{
		stats_variances_buffer = vector<double>();
		stats_variances = vector<double>();
	}	

	stats_allocate = 0;
	stats_nbMiss = 0;
	stats_nbHits = 0;

	stats_update_learning = 0, stats_update = 0;
	stats_allocate = 0, stats_allocate_learning = 0;
}


PerceptronPredictor::~PerceptronPredictor()
{
	for(auto feature : m_BP_features)
		delete feature;
}

void
PerceptronPredictor::initFeatures(bool isBP)
{
	vector<FeatureTable*>* features;
	vector<string>* criterias_name;
	vector<hashing_function>* features_hash;

	if(isBP)
	{
		criterias_name = &m_BP_criterias_names;
		features_hash = &m_BP_features_hash;
		features = &m_BP_features;		
	}
	else
	{
		criterias_name = &m_Allocation_criterias_names;
		features_hash = &m_Allocation_features_hash;
		features = &m_Allocation_features;
	}

	for(auto feature : *criterias_name)
	{
		features->push_back( new FeatureTable(m_tableSize, feature, isBP));
		if(feature == "MissPC_LSB")
			features_hash->push_back(hashingMissPC_LSB);
		else if(feature == "MissPC_LSB1")
			features_hash->push_back(hashingMissPC_LSB1);
		else if(feature == "MissPC_MSB")
			features_hash->push_back(hashingMissPC_MSB);		

		else if(feature == "TagBlock")
			features_hash->push_back(hashingTag_block);
		else if(feature == "TagPage")
			features_hash->push_back(hashingTag_page);
	
		else if(feature == "MissCounter")
			features_hash->push_back(hashing_MissCounter);	
		else if(feature == "MissCounter1")
			features_hash->push_back(hashing_MissCounter1);
		
		else if( feature == "CallStack")
			features_hash->push_back(hashing_CallStack);
		else if( feature == "CallStack1")	
			features_hash->push_back(hashing_CallStack1);
			
		else if(feature == "currentPC")
			features_hash->push_back(hashing_currentPC);
		else if(feature == "currentPC1")
			features_hash->push_back(hashing_currentPC1);
		else if(feature == "currentPC_3")
			features_hash->push_back(hashingcurrentPC_3);
		else if(feature == "currentPC_2")
			features_hash->push_back(hashingcurrentPC_2);
		else if(feature == "currentPC_1")
			features_hash->push_back(hashingcurrentPC_1);
		else
			assert(false && "Error while initializing the features , wrong name\n");


		if(feature == "CallStack" || feature == "CallStack1")
			m_need_callee_file = true;
	}	
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
	
	if(!isLearning && m_enableBP)
	{
		//Predict if we should bypass
		int sum_prediction = 0;
		vector<double> samples;
		for(unsigned i = 0 ; i < m_BP_features.size(); i++)
		{
			int hash = m_BP_features_hash[i](element.m_address , element.m_pc);
			int pred = m_BP_features[i]->getConfidence(hash,true);
			samples.push_back(pred);
			sum_prediction += pred;
		}

		if(simu_parameters.perceptron_compute_variance)
		{	//Compute Variance of the sample
			double variance = 0, avg = (double)sum_prediction / (double) samples.size();
			for(unsigned i = 0 ; i < samples.size(); i++)
				variance += (samples[i] - avg) * (samples[i] - avg);

			variance = sqrt( variance / (double) samples.size());
			stats_variances_buffer.push_back(variance);
		}		
		
		if(sum_prediction > simu_parameters.perceptron_bypass_threshold)
		{
			stats_nbBPrequests[stats_nbBPrequests.size()-1]++;
			return BYPASS_CACHE;
		}
	}

	if(m_enableAllocation)
	{
		//Bypass decision is done, we do not bypass
		//Decide NVM/SRAM allocation
		int sum_prediction = 0;
		for(unsigned i = 0 ; i < m_Allocation_features.size(); i++)
		{
			int hash = m_Allocation_features_hash[i](element.m_address , element.m_pc);
			int pred = m_Allocation_features[i]->getConfidence(hash, true);
			sum_prediction += pred;
		}	
	
		if(sum_prediction > simu_parameters.perceptron_allocation_threshold)
			return ALLOCATE_IN_NVM;
		else
			return ALLOCATE_IN_SRAM;
	}
	else 
		return ALLOCATE_IN_SRAM;
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
		if(m_enableBP)
		{
			stats_update_learning++;
			for(unsigned i = 0 ; i < m_BP_features.size() ; i++)
			{
				if( entry->perceptron_BPpred[i] > -simu_parameters.perceptron_bypass_learning || !entry->predictedReused[i])
					m_BP_features[i]->decreaseConfidence(entry->perceptron_BPHash[i]);
			}
			setBPPrediction(entry);
		}
		
		if(m_enableAllocation)
		{
			RD_TYPE rd_type = classifyRD(set , index , inNVM);
			
		
			if(rd_type == RD_SHORT && element.isWrite() && inNVM)
			{
				for(unsigned i = 0 ; i < m_Allocation_features.size() ; i++)
				{
					if( entry->perceptron_Allocpred[i] > - simu_parameters.perceptron_bypass_learning)
						m_Allocation_features[i]->decreaseConfidence(entry->perceptron_AllocHash[i]);
				}		
			}
			setAllocPrediction(entry);

		}
	}
	
	if(m_enableAllocation)
	{
		RD_TYPE rd_type = classifyRD(set , index , inNVM);
	
		for(unsigned i = 0 ; i < m_Allocation_features.size() ; i++)
		{
			int hash = m_Allocation_features_hash[i](entry->address , entry->m_pc);
			m_Allocation_features[i]->recordAccess( hash  , element.isWrite() , rd_type);
		}
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

	/** Training learning cache lines */ 
	if(current->isLearning)
	{
		if(m_enableBP)
		{
			stats_allocate_learning++;
			setBPPrediction(current);		
		}
		
		if(m_enableAllocation)
		{
			if(element.isSRAMerror && !inNVM)
			{
				for(unsigned i = 0 ; i < m_Allocation_features.size() ; i++)
				{
					if( current->perceptron_Allocpred[i] < simu_parameters.perceptron_bypass_learning)
						m_Allocation_features[i]->increaseConfidence(current->perceptron_AllocHash[i]);
				}
			}
		}
	}
	
	if(m_enableAllocation)
	{
		RD_TYPE rd_type = classifyRD(set , index , inNVM);
		if( element.isSRAMerror )
			rd_type = RD_MEDIUM;
		
		for(unsigned i = 0 ; i < m_Allocation_features.size() ; i++)
		{
			int hash = m_Allocation_features_hash[i](current->address , current->m_pc);
			m_Allocation_features[i]->recordAccess( hash  , element.isWrite() , rd_type);
		}
	}
			
	Predictor::insertionPolicy(set , index , inNVM , element);
}


void
PerceptronPredictor::update_globalPChistory(uint64_t pc)
{	
	m_global_PChistory.push_front(pc);
	if( m_global_PChistory.size() == 11)
		m_global_PChistory.pop_back();
	
	if(!m_need_callee_file)
		return;
	
	if(m_callee_map.count(pc) != 0)
	{
		m_callee_PChistory.push_front(pc);
		if( m_callee_PChistory.size() == 11)
			m_callee_PChistory.pop_back();
	}
}

void
PerceptronPredictor::setAllocPrediction(CacheEntry *current)
{
	int alloc_pred=0;
	for(unsigned i = 0 ; i < m_Allocation_features.size() ; i++)
	{
		int hash = m_Allocation_features_hash[i](current->address , current->m_pc);
		alloc_pred = m_Allocation_features[i]->getConfidence(hash, true);
		current->perceptron_Allocpred[i] = alloc_pred;
		current->perceptron_AllocHash[i] = hash;
//		if(alloc_pred > simu_parameters.perceptron_allocation_threshold)
//			current->predictedReused[i] = false;
//		else
//			current->predictedReused[i] = true;
	}
}


void 
PerceptronPredictor::setBPPrediction(CacheEntry* current)
{
	int bp_pred=0;
	for(unsigned i = 0 ; i < m_BP_features.size() ; i++)
	{
		int hash = m_BP_features_hash[i](current->address , current->m_pc);
		bp_pred = m_BP_features[i]->getConfidence(hash,true);
		current->perceptron_BPpred[i] = bp_pred;
		current->perceptron_BPHash[i] = hash;
		if(bp_pred > simu_parameters.perceptron_bypass_threshold)
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
		for(unsigned i =  0 ; i < m_BP_features.size() ; i++)
		{
			if(current->predictedReused[i] || current->perceptron_BPpred[i] < simu_parameters.perceptron_bypass_learning)
			{
				int hash = m_BP_features_hash[i](current->address , current->m_pc);
 				m_BP_features[i]->increaseConfidence(hash);
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
	
	ofstream file(FILE_OUTPUT_PERCEPTRON);	
	for(unsigned i = 0; i < stats_variances.size() ; i++)
		file << stats_variances[i] << endl;
	file.close();
	
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
	
	string a = m_enableAllocation ? "TRUE" : "FALSE";
	out << entete << ":Perceptron:EnableAllocation\t" << a << endl;
	a = m_enableBP ? "TRUE" : "FALSE";
	out << entete << ":Perceptron:EnableBP\t" << a << endl;
	
	out << entete << ":Perceptron:BypassThreshold\t" << simu_parameters.perceptron_bypass_threshold  << endl;
	out << entete << ":Perceptron:BypassLearning\t" << simu_parameters.perceptron_bypass_learning << endl; 
	out << entete << ":Perceptron:AllocThreshold\t" << simu_parameters.perceptron_allocation_threshold  << endl;
	out << entete << ":Perceptron:AllocLearning\t" << simu_parameters.perceptron_allocation_learning << endl; 
	out << entete << ":Perceptron:BypassFeatures\t"; 
	for(auto p : m_BP_criterias_names)
		out << p << ",";
	out << endl;	
	out << entete << ":Perceptron:AllocFeatures\t"; 
	for(auto p : m_Allocation_criterias_names)
		out << p << ",";
	out << endl;	

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

void
PerceptronPredictor::openNewTimeFrame()
{
	stats_nbBPrequests.push_back(0);
	stats_nbDeadLine.push_back(0);
	
	stats_missCounter.push_back( miss_counter );
	stats_nbHits = 0, stats_nbMiss = 0;

	if(simu_parameters.perceptron_compute_variance)
	{
		if(stats_variances_buffer.size() != 0)
		{
			double avg = 0;
			for(unsigned i = 0 ; i < stats_variances_buffer.size() ; i++)
				avg += stats_variances_buffer[i];
			avg /= (double)stats_variances_buffer.size();

			stats_variances.push_back(avg);	
			stats_variances_buffer.clear();
		}
	}

	for(auto feature : m_BP_features)
		feature->openNewTimeFrame();
	
	
	Predictor::openNewTimeFrame();
}

void 
PerceptronPredictor::finishSimu()
{
	
	for(auto feature : m_BP_features)
		feature->finishSimu();

	Predictor::finishSimu();
}


void 
PerceptronPredictor::load_callee_file()
{

	string mem_trace = simu_parameters.memory_traces[0];
	string dir_name = splitFilename(mem_trace);
	string callee_filename = dir_name + "/callee_recap.txt"; 
	
	ifstream file(callee_filename);
	
	assert(file && "Callee file not found\n");
	string line;
	
	while( getline(file, line))
	{		
		m_callee_map.insert(pair<uint64_t,int>( hexToInt(line) , 0));		
	};
		
	file.close();	
}



RD_TYPE
PerceptronPredictor::classifyRD(int set , int index, bool inNVM)
{
	vector<CacheEntry*> line;
	int64_t ref_rd = 0;
	if(inNVM)
	{
		ref_rd = m_tableNVM[set][index]->policyInfo;
		line = m_tableNVM[set];
	}
	else
	{
		line = m_tableSRAM[set];
		ref_rd = m_tableSRAM[set][index]->policyInfo;
	}
	
	int position = 0;
	/* Determine the position of the cache line in the LRU stack */
	for(unsigned i = 0 ; i < line.size() ; i ++)
	{
		if(line[i]->policyInfo < ref_rd && line[i]->isValid)
			position++;
	}	

	if(simu_parameters.nvm_assoc > simu_parameters.sram_assoc)
	{
		if(position < simu_parameters.sram_assoc)
			return RD_SHORT;
		else
			return RD_MEDIUM;	
	}
	else
	{
		if(position < simu_parameters.nvm_assoc)
			return RD_SHORT;
		else
			return RD_MEDIUM;
	}
}


