#include <cmath>
#include <vector>
#include <map>
#include <ostream>

#include "Cerebron.hh"

using namespace std;


deque<uint64_t> CerebronPredictor::m_global_PChistory;
deque<uint64_t> CerebronPredictor::m_callee_PChistory;

ofstream debug_file;

CerebronPredictor::CerebronPredictor(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) :\
	Predictor(id, nbAssoc , nbSet , nbNVMways , SRAMtable , NVMtable , cache)
{	
	m_tableSize = simu_parameters.perceptron_table_size;

	m_features.clear();

	m_criterias_names = simu_parameters.PHC_features;

	m_need_callee_file = false;
	
	m_enableAllocation = true;
	if(simu_parameters.nvm_assoc == 0 || simu_parameters.sram_assoc == 0)
		m_enableAllocation = false;
	
	initFeatures();
	
	cout <<  "\tFeatures used : ";
	for(auto p : simu_parameters.PHC_features)
		cout << p << ",";
	cout << endl;


	m_activation_function = simu_parameters.Cerebron_activation_function;
	if(m_activation_function != "linear" && m_activation_function != "max")
		assert(false && "Wrong Activation function for the Cerebron predictor");
	
	if(m_need_callee_file)
	{
		m_callee_map = map<uint64_t,int>();
		load_callee_file();
	}
		
	m_costAccess = vector< vector<int> >(NUM_RW_TYPE , vector<int>(NUM_RD_TYPE, 0));
	m_costAccess[true][RD_MEDIUM] = simu_parameters.PHC_cost_mediumWrite;
	m_costAccess[false][RD_MEDIUM] = simu_parameters.PHC_cost_mediumRead;
	m_costAccess[true][RD_SHORT] = simu_parameters.PHC_cost_shortWrite;
	m_costAccess[false][RD_SHORT] = simu_parameters.PHC_cost_shortRead;
	
	m_costParameters = simu_parameters.optTarget;
	
	miss_counter = 0;
	m_cpt = 0;

	m_global_PChistory = deque<uint64_t>();
	m_callee_PChistory = deque<uint64_t>();
	if(simu_parameters.printDebug)
		debug_file.open(CEREBRON_DEBUG_FILE);
		
	stats_nbBPrequests = vector<uint64_t>(1, 0);
	stats_nbDeadLine = vector<uint64_t>(1, 0);
	stats_missCounter = vector<int>();
	stats_access_class = vector<vector<int> >(NUM_RD_TYPE ,  vector<int>(2, 0));
	
	
	if(simu_parameters.perceptron_drawFeatureMaps)
	{
		stats_local_error = vector< vector< pair<int,int> > >(simu_parameters.PHC_features.size() \
		 , vector< pair<int,int> >(m_tableSize, pair<int,int>(0,0)));
		stats_global_error = vector< vector< pair<int,int> > >(m_tableSize , \
			vector< pair<int,int> >(m_tableSize , pair<int,int>(0,0)));
	}
	
	if(simu_parameters.perceptron_compute_variance)
	{
		stats_variances_buffer = vector<double>();
		stats_variances = vector<double>();
	}	

	stats_allocate = 0;
	stats_nbMiss = 0;
	stats_nbHits = 0;

	stats_good_alloc = 0, stats_wrong_alloc = 0;

	stats_update_learning = 0, stats_update = 0;
	stats_allocate = 0, stats_allocate_learning = 0;
	stats_prediction_preemptive = 0 , stats_prediction_confident = 0;
	stats_cptLearningSRAMerror = 0, stats_cptSRAMerror = 0;

}


CerebronPredictor::~CerebronPredictor()
{
	for(auto feature : m_features)
		delete feature;
}

void
CerebronPredictor::initFeatures()
{
	vector<string>* criterias_name = &m_criterias_names;
	vector<hashing_function>* features_hash = &m_features_hash;
	vector<FeatureTable*>* features = &m_features;		

	for(auto feature : *criterias_name)
	{
		features->push_back( new FeatureTable(m_tableSize, feature, false));
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
CerebronPredictor::allocateInNVM(uint64_t set, Access element)
{
//	if(element.isInstFetch())
//		return ALLOCATE_IN_NVM;
	
	update_globalPChistory(element.m_pc);
	stats_nbMiss++;
	miss_counter++;
	if(miss_counter > 255)
		miss_counter = 255;
	
	// All the set is a learning/sampled set independantly of its way or SRAM/NVM alloc
	//bool isLearning = m_tableSRAM[set][0]->isLearning; 
		
	return activationFunction(element);
}


void
CerebronPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* entry = get_entry(set , index , inNVM);
	entry->missPC = element.m_pc;
	RD_TYPE rd = RD_NOT_ACCURATE;
	if( element.isSRAMerror)
	{
		stats_cptSRAMerror++;
		rd = RD_MEDIUM;	
	}
	entry->policyInfo = m_cpt++;
	
	recordAccess(entry, element.block_addr, entry->missPC, set , element.isWrite() , inNVM, index, rd);

	if(entry->isLearning)
	{
		if( simu_parameters.Cerebron_RDmodel)
		{
			entry->e_sram = m_costParameters.costSRAM[element.isWrite()];
			entry->e_nvm = m_costParameters.costNVM[element.isWrite()];
		}
		else
			entry->cost_value = m_costAccess[element.isWrite()][RD_SHORT];	
		/*
		if( element.isSRAMerror)
		{
			entry->cost_value = missingTagCostValue(element.block_addr , set) +  m_costAccess[element.isWrite()][RD_MEDIUM];
			stats_cptLearningSRAMerror++;
		}
		else
			entry->cost_value = m_costAccess[element.isWrite()][RD_SHORT];
		*/
	}

	if(simu_parameters.printDebug)
	{
		int hash = m_features_hash[0](element.block_addr , entry->missPC);	
		reportAccess( m_features[0]->getEntry(hash) , element, entry , entry->isNVM, string("INSERTION"), string(str_RD_status[rd]), hash);
	}
	
	
				
	stats_allocate++;
	stats_access_class[rd][element.isWrite()]++;
	Predictor::insertionPolicy(set , index , inNVM , element);
}


void
CerebronPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest)
{
	CacheEntry *entry = get_entry(set , index , inNVM);
	RD_TYPE rd = classifyRD( set , index , inNVM );
	stats_access_class[rd][element.isWrite()]++;

	recordAccess( entry , element.block_addr, entry->missPC, set, element.isWrite() , inNVM, index, rd);
	
	entry->policyInfo = m_cpt++;
	update_globalPChistory(element.m_pc);

	if(entry->isLearning)
	{
//		cout << "UpdatePolicy Learning Cache line 0x" << std::hex << element.block_addr
//			<< std::dec << " Cost Value " << entry->cost_value << endl;
		if(simu_parameters.Cerebron_fastlearning)
			doLearning(entry, inNVM);
	}

	if(miss_counter < 0)
		miss_counter = 0;

	stats_update++;
	stats_nbHits++;
	miss_counter--;

	if(simu_parameters.printDebug)
	{
		int hash = m_features_hash[0](element.block_addr , entry->missPC);	
		reportAccess( m_features[0]->getEntry(hash) , element, entry , entry->isNVM, string("UPDATE"), string(str_RD_status[rd]), hash);
	}

	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}

int CerebronPredictor::evictPolicy(int set, bool inNVM)
{
	int assoc_victim = -1;
	assert(m_replacementPolicyNVM_ptr != NULL);
	assert(m_replacementPolicySRAM_ptr != NULL);

	CacheEntry* entry = NULL;
	if(inNVM)
	{	
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
		entry = m_tableNVM[set][assoc_victim];
	}
	else
	{
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);
		entry = m_tableSRAM[set][assoc_victim];		
	}

	if(!entry->isValid)
		return assoc_victim;

	allocDecision des = getDecision(entry);
	if(simu_parameters.perceptron_drawFeatureMaps)
	{
		bool global_error = false;
		
		if(( des == ALLOCATE_IN_SRAM && inNVM ) || \
			( des == ALLOCATE_IN_NVM && !inNVM ))
			global_error = true;

		int hash1 = m_features_hash[0](entry->address , entry->missPC);
		int hash2 = (m_features.size() > 1) ? m_features_hash[1](entry->address , entry->missPC) : 0;
		if(global_error)
			stats_global_error[hash1][hash2].first++;
		else
			stats_global_error[hash1][hash2].second++;
		
	
		for(unsigned i = 0; i < m_features.size() ; i++)
		{
			int hash = m_features_hash[i](entry->address , entry->missPC);

			bool local_error = false;
			if( des == ALLOCATE_IN_NVM && entry->PHC_allocation_pred[i].second == ALLOCATE_IN_SRAM)
				local_error = true;
			else if(  des == ALLOCATE_IN_SRAM && entry->PHC_allocation_pred[i].second == ALLOCATE_IN_NVM )
			 	local_error = true;
			 					
			if(local_error)	
				stats_local_error[i][hash].first++;
			else 
				stats_local_error[i][hash].second++;
				
		}

	}
		
	if( (des == ALLOCATE_IN_NVM && !inNVM) || \
		(des == ALLOCATE_IN_SRAM && inNVM) )
	{
		stats_wrong_alloc++;
	}
	else
		stats_good_alloc++;
		
	/* Trigger the learning process */ 
	if( entry->isLearning && !simu_parameters.Cerebron_fastlearning)
		doLearning(entry, inNVM);
	

	if(entry->nbWrite == 0 && entry->nbRead == 0)
		stats_nbDeadLine[stats_nbDeadLine.size()-1]++;

	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}


void
CerebronPredictor::evictRecording( int id_set , int id_assoc , bool inNVM)
{ 
	Predictor::evictRecording(id_set, id_assoc, inNVM);
}

void 
CerebronPredictor::doLearning(CacheEntry* entry, bool inNVM)
{
	allocDecision des = getDecision(entry);
	if( simu_parameters.Cerebron_independantLearning)
	{
		for(unsigned i = 0; i < m_features.size() ; i++)
		{
			if( (des == ALLOCATE_IN_NVM && entry->PHC_allocation_pred[i].second == ALLOCATE_IN_SRAM ) || \
			( des == ALLOCATE_IN_SRAM && entry->PHC_allocation_pred[i].second == ALLOCATE_IN_NVM) )
				m_features[i]->decreaseConfidence(entry->PHC_allocation_pred[i].first);
			else 
				m_features[i]->increaseConfidence(entry->PHC_allocation_pred[i].first);
		}
	}
	else
	{		
		if( ( des == ALLOCATE_IN_NVM && !inNVM) || \
			(des == ALLOCATE_IN_SRAM && inNVM) )
		{
			for(unsigned i = 0; i < m_features.size() ; i++)
			{
				int hash = m_features_hash[i](entry->address , entry->missPC);
				m_features[i]->decreaseConfidence(hash);
			}
		}			
		else
		{
			for(unsigned i = 0; i < m_features.size() ; i++)
			{
				int hash = m_features_hash[i](entry->address , entry->missPC);
				m_features[i]->increaseConfidence(hash);
			}
		}
	}
}

allocDecision 
CerebronPredictor::getDecision(CacheEntry* entry)
{
	if(simu_parameters.Cerebron_RDmodel)
	{
		if( entry->e_sram > entry->e_nvm )
			return ALLOCATE_IN_SRAM;
		else 
			return ALLOCATE_IN_NVM;
	}
	else
	{
		if( entry->cost_value > 0)
			return ALLOCATE_IN_SRAM;
		else
			return ALLOCATE_IN_NVM;
	}
}

void
CerebronPredictor::recordAccess(CacheEntry* entry,uint64_t block_addr, uint64_t missPC, int set, bool isWrite , bool inNVM, int index, RD_TYPE rd)
{
	if(simu_parameters.Cerebron_RDmodel)
	{
		bool hitSRAM = false, hitNVM = false;
		hitSRAM = getHitPerDataArray( block_addr , set , false);
		hitNVM = getHitPerDataArray( block_addr , set , true);

		entry->e_sram += m_costParameters.costSRAM[isWrite];
		entry->e_sram +=  hitSRAM ? m_costParameters.costDRAM[isWrite] : 0;

		entry->e_nvm += m_costParameters.costNVM[isWrite];
		entry->e_nvm += hitNVM ? m_costParameters.costDRAM[isWrite] : 0;
	
		if(entry->isLearning)
		{
			for(unsigned i = 0 ; i < m_features.size() ; i++)
			{
				int hash = m_features_hash[i](block_addr , missPC);
				m_features[i]->recordAccess(hash, isWrite , hitSRAM , hitNVM);
			}		
		}
	}
	else
	{
		entry->cost_value += m_costAccess[isWrite][rd];
		if(entry->isLearning)
		{
			for(unsigned i = 0 ; i < m_features.size() ; i++)
			{
				int hash = m_features_hash[i]( block_addr , missPC);
				m_features[i]->recordAccess(hash, isWrite , rd);	
			}		
		}
	}	
}

allocDecision
CerebronPredictor::activationFunction(Access element)
{

	if(m_activation_function == "linear")
	{
	
		int sum_pred = 0;
		for(unsigned i = 0 ; i < m_features.size() ; i++)
		{
			int hash = m_features_hash[i](element.block_addr , element.m_pc);
			int confidence = m_features[i]->getConfidence(hash, true);
			allocDecision des = m_features[i]->getAllocDecision(hash, element.isWrite());

			if(simu_parameters.printDebug)
				debug_file << "ID Dataset:" << hash << "\t" << element.m_pc << endl;

		
			if( des == ALLOCATE_IN_SRAM )
				sum_pred += (-1) * confidence;
			else
				sum_pred += confidence;
		}
		return convertToAllocDecision(sum_pred, element.isWrite());
	}
	else if(m_activation_function == "max")
	{
		allocDecision des = ALLOCATE_IN_SRAM;
		int max_confidence = 0;
		for(unsigned i = 0 ; i < m_features.size() ; i++)
		{
			int hash = m_features_hash[i](element.block_addr , element.m_pc);
			int confidence = m_features[i]->getConfidence(hash, true);
			
			if(max_confidence < confidence)
			{
				max_confidence = confidence;
				des = m_features[i]->getAllocDecision(hash, element.isWrite());
			}
			return des;
		}
	}
	
	assert(false && "Error, should have return before");
}

allocDecision
CerebronPredictor::convertToAllocDecision(int alloc_counter, bool isWrite)
{

	if( abs(alloc_counter) < simu_parameters.perceptron_allocation_threshold )
	{
		if( isWrite )
			return ALLOCATE_IN_SRAM;
		else 
			return ALLOCATE_IN_NVM;
	}
	else
	{
		if( alloc_counter < simu_parameters.perceptron_allocation_threshold )
			return ALLOCATE_IN_SRAM;
		else
			return ALLOCATE_IN_NVM;
	}
}


void
CerebronPredictor::update_globalPChistory(uint64_t pc)
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
CerebronPredictor::printStats(std::ostream& out, std::string entete) { 

	uint64_t total_BP = 0, total_DeadLines = 0;
	for(unsigned i = 0 ; i < stats_nbBPrequests.size() ; i++ )
	{
		total_BP+= stats_nbBPrequests[i];		
		total_DeadLines += stats_nbDeadLine[i];
	}
	/*
	ofstream file(FILE_OUTPUT_Cerebron);	
	for(unsigned i = 0 ; i < stats_histo_value.size() ; i++)
	{
		file << "Feature : " <<  m_criterias_names[i] << endl;
		for(unsigned j = 0 ; j < stats_histo_value[i].size() ; j++)
		{
			file << "Index " << j << "\t";
			for(auto cost : stats_histo_value[i][j])
				file << cost << ", ";
			file << endl;
		}
		
		
		file << "-------------" << endl;
		file << endl;
	}
	
	file.close();
	*/
	out << entete << ":CerebronPredictor:PredictionPreemptive\t" << stats_prediction_preemptive << endl;
	out << entete << ":CerebronPredictor:PredictionConfident\t" << stats_prediction_confident << endl;
	out << entete << ":CerebronPredictor:NBBypass\t" << total_BP << endl;
	out << entete << ":CerebronPredictor:TotalDeadLines\t" << total_DeadLines << endl;
	out << entete << ":CerebronPredictor:SRAMerror\t" << stats_cptSRAMerror << endl;
	out << entete << ":CerebronPredictor:LearningSRAMerror\t" << stats_cptLearningSRAMerror << endl;
	out << entete << ":CerebronPredictor:WrongAlloc\t" << stats_wrong_alloc << endl;
	out << entete << ":CerebronPredictor:CorrectAlloc\t" << stats_good_alloc << endl;

	
	out << entete << ":CerebronPredictor:AccessClassification" << endl;
	for(unsigned i = 0 ; i < stats_access_class.size() ; i++)
	{
		if(stats_access_class[i][0] != 0 || stats_access_class[i][1] != 1)
		{
			out << entete << ":CerebronPredictor:Read" <<  str_RD_status[i]	<< "\t" << stats_access_class[i][false] << endl;	
			out << entete << ":CerebronPredictor:Write" << str_RD_status[i] << "\t" << stats_access_class[i][true] << endl; 		
		}
	}

	Predictor::printStats(out, entete);
}



void CerebronPredictor::printConfig(std::ostream& out, std::string entete) { 

	out << entete << ":Cerebron:TableSize\t" << m_tableSize << endl; 
	
	string a = m_enableAllocation ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:EnableAllocation\t" << a << endl;
	
	out << entete << ":Cerebron:CostValueThreshold\t" << simu_parameters.PHC_cost_threshold  << endl;
	out << entete << ":Cerebron:AllocationThreshold\t" << simu_parameters.perceptron_allocation_threshold  << endl;
	out << entete << ":Cerebron:ActivationFunction\t" << simu_parameters.Cerebron_activation_function  << endl;
	string b = simu_parameters.Cerebron_independantLearning ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:IndependantLearning\t" << b  << endl;
	string c = simu_parameters.Cerebron_fastlearning ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:FastLearning\t" << c << endl;
	
	string d = simu_parameters.Cerebron_RDmodel ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:UsingRDModel\t" << d << endl;

//	out << entete << ":Cerebron:CostLearning\t" << simu_parameters.perceptron_allocation_learning << endl; 
	out << entete << ":Cerebron:AllocFeatures\t"; 
	for(auto p : m_criterias_names)
		out << p << ",";
	out << endl;	

	out << entete << ":CerebronPredictor:CostModel" << endl;
	out << entete << ":CerebronPredictor:ShortRead\t" << m_costAccess[false][RD_SHORT] << endl; 		
	out << entete << ":CerebronPredictor:ShortWrite\t" << m_costAccess[true][RD_SHORT] << endl; 		
	out << entete << ":CerebronPredictor:MediumRead\t" << m_costAccess[false][RD_MEDIUM] << endl; 		
	out << entete << ":CerebronPredictor:MediumWrite\t" << m_costAccess[true][RD_MEDIUM] << endl; 		


	Predictor::printConfig(out, entete);
}

CacheEntry*
CerebronPredictor::get_entry(uint64_t set , uint64_t index , bool inNVM)
{
	if(inNVM)
		return m_tableNVM[set][index];
	else
		return m_tableSRAM[set][index];
}

void
CerebronPredictor::openNewTimeFrame()
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

	for(auto feature : m_features)
		feature->openNewTimeFrame();
	
	
	Predictor::openNewTimeFrame();
}

void 
CerebronPredictor::finishSimu()
{
	if(simu_parameters.perceptron_drawFeatureMaps)
	{
		drawFeatureMaps();
	}
	
	for(auto feature : m_features)
		feature->finishSimu();

	Predictor::finishSimu();
}


void
CerebronPredictor::drawFeatureMaps()
{

	ofstream file("output_PHC.out");
	for(unsigned i = 0 ; i < stats_global_error.size() ; i++)
	{
		for(unsigned j =  0 ; j < stats_global_error[i].size() ; j++)
		{
			double a = stats_global_error[i][j].first;
//			double b = stats_global_error[i][j].second;
//			double dummy = 0;
//			if( !(a == 0 && b == 0) )
//				dummy = a / (a + b);

			file << a << ",";
		}
		file << endl;
	}
	file.close();

	ofstream file1("output_PHC1.out");
	for(unsigned i = 0 ; i < stats_local_error.size() ; i++)
	{
		for(unsigned j =  0 ; j < stats_local_error[i].size() ; j++)
		{
			double a = stats_local_error[i][j].first;
//			double b = stats_local_error[i][j].second;
//			double dummy = a / (a + b);
			file1 << a << ",";
		}
		file1 << endl;
	}
	file1.close();

}

void 
CerebronPredictor::load_callee_file()
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
CerebronPredictor::classifyRD(int set , int index, bool inNVM)
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
//	cout << "classifyRD, cpt_time =  " << cpt_time << ", set = " << set << " col = " << index << ", ref_rd = " << ref_rd << endl;
	for(unsigned i = 0 ; i < line.size() ; i ++)
	{
//		cout << "\tPolicy[" << i << "] = " << line[i]->policyInfo << endl;
		if(line[i]->policyInfo > ref_rd && line[i]->isValid)
			position++;
	}	
//	cout << "Position = " << position << endl;

	if(simu_parameters.nvm_assoc > simu_parameters.sram_assoc)
	{
		if(position < simu_parameters.mediumrd_def)
			return RD_SHORT;
		else
			return RD_MEDIUM;	
	}
	else
	{
		if(position < simu_parameters.mediumrd_def)
			return RD_SHORT;
		else
			return RD_MEDIUM;
	}
}


void
CerebronPredictor::reportAccess(FeatureEntry* feature_entry, Access element, CacheEntry* current, bool inNVM, string entete, string reuse_class, int hash)
{
	string cl_location = inNVM ? "NVM" : "SRAM";
	string is_learning = current->isLearning ? "Learning" : "Regular";
	string access_type = element.isWrite() ? "WRITE" : "READ";
	
	string is_error= "";
	if(element.isSRAMerror)
		is_error = ", is an SRAM error";
	
	if(simu_parameters.printDebug)
	{
		debug_file << entete << ":Dataset n°" << hash << ": [" \
		<< "] = " << str_allocDecision[feature_entry->des] << "," \
		<< access_type << " on " << is_learning << " Cl 0x" << std::hex << current->address \
		<< std::dec << " allocated in " << cl_location<< ", Reuse=" << reuse_class \
		<< " , " << is_error << ", " << endl;	
	}
}


