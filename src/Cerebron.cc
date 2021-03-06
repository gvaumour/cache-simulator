#include <cmath>
#include <vector>
#include <map>
#include <ostream>

#include "Cerebron.hh"

using namespace std;

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


	stats_nbMigrations = vector<int>(2,0);
	
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
	vector<params_hash>* features_hash = &m_features_hash;
	vector<FeatureTable*>* features = &m_features;		

	for(auto feature : *criterias_name)
	{
		features->push_back( new FeatureTable(m_tableSize, feature, false));
		features_hash->push_back( parseFeatureName(feature) );
		/*
		if(feature == "CallStack" || feature == "CallStack1")
			m_need_callee_file = true;
		*/
	}	
}


allocDecision
CerebronPredictor::allocateInNVM(uint64_t set, Access element)
{
//	if(element.isInstFetch())
//		return ALLOCATE_IN_NVM;
	
	update_globalPChistory(element.m_pc);
	if( hitInSRAMMissingTags( element.block_addr, set) )
		element.m_pc = missingTagMissPC( element.block_addr , set);
		
	return activationFunction(element.isWrite() , element.block_addr , element.m_pc);
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
			

	if(entry->isLearning_policy)
		recordAccess(entry, element.block_addr, entry->missPC, set , element.isWrite() , inNVM, index, rd , false);

	if(entry->isLearning_weight)
	{
		if( element.isSRAMerror)
		{
			debug_file << "SRAM error detected on Block Addr = 0x" \
					<< std::hex << element.block_addr << std::dec << endl;
			
			uint64_t missPC = missingTagMissPC(element.block_addr , set);
			uint64_t cost_value = missingTagCostValue(element.block_addr , set);
			entry->missPC = missPC;
			for(unsigned i = 0 ; i < m_features.size() ; i++)
			{
				int hash = hashing_function1(m_features_hash[i] , element.block_addr , entry->missPC);
				m_features[i]->decreaseConfidence(hash);
				if(cost_value > 0)
					m_features[i]->decreaseConfidence(hash);
			}
			stats_cptLearningSRAMerror++;
		}
	}
	
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		pair<int , allocDecision> dummy;
		dummy.first = hashing_function1(m_features_hash[i] , element.block_addr , entry->missPC);
		dummy.second = m_features[i]->getAllocDecision(dummy.first, false);
		entry->PHC_allocation_pred[i] = dummy;
	}		
	
	if( !simu_parameters.Cerebron_RDmodel)
	{
		entry->cost_value = m_costAccess[element.isWrite()][RD_SHORT];
		if( element.isSRAMerror)
			entry->cost_value += m_costAccess[element.isWrite()][RD_MEDIUM];		

	}

	reportAccess( element, entry , set,  entry->isNVM, string("INSERTION"), string(str_RD_status[rd]));
				
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
	
	entry->policyInfo = m_cpt++;
	if ( element.isDemandAccess())
		update_globalPChistory(element.m_pc);

	entry->cost_value += m_costAccess[element.isWrite()][rd];
	if(entry->isLearning_policy)
	{
		recordAccess( entry , element.block_addr, entry->missPC, set, element.isWrite() , inNVM, index, rd , true);
	}
	if(entry->isLearning_weight)
	{
		if(simu_parameters.Cerebron_fastlearning)
			doLearning(entry, inNVM);
	}

	if(miss_counter < 0)
		miss_counter = 0;

	stats_update++;
	stats_nbHits++;
	miss_counter--;

	if(simu_parameters.Cerebron_enableMigration)
	{
		assert(entry->isValid);
		entry = checkLazyMigration(entry , set , inNVM , index, element.isWrite());		
	}

	reportAccess( element, entry , set , entry->isNVM, string("UPDATE"), string(str_RD_status[rd]) );

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

		int hash1 = hashing_function1(m_features_hash[0] , entry->address , entry->missPC);
		int hash2 = (m_features.size() > 1) ? hashing_function1(m_features_hash[1], entry->address , entry->missPC) : 0;
		if(global_error)
			stats_global_error[hash1][hash2].first++;
		else
			stats_global_error[hash1][hash2].second++;
		
	
		for(unsigned i = 0; i < m_features.size() ; i++)
		{
			int hash = hashing_function1( m_features_hash[i] , entry->address , entry->missPC);

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
		if(simu_parameters.printDebug)
		{
			string a = inNVM ? "NVM" : "SRAM"; 
			debug_file << cpt_time << ": Eviction of 0x" << std::hex  << entry->address \
				   << std::dec << " , wrongly allocated in " << a << ", cost=" << entry->cost_value << endl;
			
			for(unsigned i = 0; i < m_features.size() ; i++)
			{
				int hash = hashing_function1( m_features_hash[i] , entry->address , entry->missPC);
				int confidence = m_features[i]->getConfidence(hash, false);
				allocDecision des = m_features[i]->getAllocDecision(hash, false);
				debug_file << "\t-Feature " << i << "\t" << str_allocDecision[des] << "\t" << confidence << endl;
			}
		}
		
		stats_wrong_alloc++;
	}
	else
		stats_good_alloc++;
		
	/* Trigger the learning process */ 
	if( entry->isLearning_weight && !simu_parameters.Cerebron_fastlearning)
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
		
		for(unsigned i = 0 ; i < m_features.size() ; i++)
		{
			pair<int , allocDecision> dummy;
			dummy.first = hashing_function1(m_features_hash[i] , entry->address , entry->missPC);
			dummy.second = m_features[i]->getAllocDecision(dummy.first, false);
			entry->PHC_allocation_pred[i] = dummy;
		}
		if(simu_parameters.Cerebron_resetEnergyValues)
		{
			entry->e_sram = 0;
			entry->e_nvm = 0;
		}
	
	}
	else
	{	
		if( ( des == ALLOCATE_IN_NVM && !inNVM) || \
			(des == ALLOCATE_IN_SRAM && inNVM) )
		{
			for(unsigned i = 0; i < m_features.size() ; i++)
			{
				int hash = hashing_function1( m_features_hash[i] , entry->address , entry->missPC);
				m_features[i]->decreaseConfidence(hash);
			}
		}			
		else
		{
			for(unsigned i = 0; i < m_features.size() ; i++)
			{
				int hash = hashing_function1 ( m_features_hash[i] , entry->address , entry->missPC);
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
			return ALLOCATE_IN_NVM;
		else 
			return ALLOCATE_IN_SRAM;
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
CerebronPredictor::recordAccess(CacheEntry* entry,uint64_t block_addr, uint64_t missPC, int set,\
			bool isWrite , bool inNVM, int index, RD_TYPE rd , bool isUpdate)
{
	if(simu_parameters.Cerebron_RDmodel)
	{
		bool hitSRAM = false, hitNVM = false;
		hitNVM = getHitPerDataArray( block_addr , set , true);
		hitSRAM = getHitPerDataArray( block_addr , set , false);
		/*
		if( !inNVM && isUpdate)
			hitSRAM = true;
		else
			hitSRAM = getHitPerDataArray( block_addr , set , false);
				
	
		if( inNVM && isUpdate)
			hitNVM = true;
		else 
			hitNVM = getHitPerDataArray( block_addr , set , true);
		*/
		/*
		string hitSRAM_str = hitSRAM ? "TRUE" : "FALSE";
		string hitNVM_str = hitNVM ? "TRUE" : "FALSE";		
		//debug_file << "hitSRAM = " << hitSRAM_str << " , hitNVM = " << hitNVM_str << endl;
		//debug_file << "BEFORE : entry->e_sram = " << entry->e_sram << endl;
		*/
		entry->e_sram += m_costParameters.costSRAM[isWrite];
		entry->e_sram +=  !hitSRAM ? m_costParameters.costDRAM[isWrite] : 0;

		entry->e_nvm += m_costParameters.costNVM[isWrite];
		entry->e_nvm += !hitNVM ? m_costParameters.costDRAM[isWrite] : 0;
		//debug_file << "END: entry->e_sram = " << entry->e_sram << endl;
	
		if(entry->isLearning_policy)
		{
			for(unsigned i = 0 ; i < m_features.size() ; i++)
			{
				int hash = hashing_function1( m_features_hash[i] , block_addr , missPC);
				m_features[i]->recordAccess(hash, isWrite , hitSRAM , hitNVM);
			}		
		}
	}
	else
	{
		if(entry->isLearning_policy)
		{
			for(unsigned i = 0 ; i < m_features.size() ; i++)
			{
				int hash = hashing_function1( m_features_hash[i] , block_addr , missPC);
				m_features[i]->recordAccess(hash, isWrite , rd);	
			}
		}
	}	
}

allocDecision
CerebronPredictor::activationFunction( bool isWrite , uint64_t block_addr , uint64_t missPC)
{

	if(m_activation_function == "linear")
	{
	
		int sum_pred = 0;
		for(unsigned i = 0 ; i < m_features.size() ; i++)
		{
			int hash = hashing_function1 ( m_features_hash[i] , block_addr , missPC);
			int confidence = m_features[i]->getConfidence(hash, true);
			allocDecision des = m_features[i]->getAllocDecision(hash, isWrite );

			if(simu_parameters.printDebug)
				debug_file << "Feature " << i << " -- ID Dataset:" << hash << "\t" << missPC << endl;

		
			if( des == ALLOCATE_IN_SRAM )
				sum_pred += (-1) * confidence;
			else
				sum_pred += confidence;
		}
		return convertToAllocDecision(sum_pred, isWrite );
	}
	else if(m_activation_function == "max")
	{
		allocDecision des = ALLOCATE_IN_SRAM;
		int max_confidence = 0;
		for(unsigned i = 0 ; i < m_features.size() ; i++)
		{
			int hash = hashing_function1( m_features_hash[i] , block_addr , missPC);
			int confidence = m_features[i]->getConfidence(hash, true);
			
			if(max_confidence < confidence)
			{
				max_confidence = confidence;
				des = m_features[i]->getAllocDecision(hash, isWrite );
			}
			return des;
		}
	}
	
	assert(false && "Error, should have return before");
}

allocDecision
CerebronPredictor::convertToAllocDecision(int alloc_counter, bool isWrite)
{
	if(simu_parameters.Cerebron_lowConfidence)
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
			if( alloc_counter < 0 )
				return ALLOCATE_IN_SRAM;
			else
				return ALLOCATE_IN_NVM;
		}
	}
	else
	{
		if( alloc_counter < 0 )
			return ALLOCATE_IN_SRAM;
		else
			return ALLOCATE_IN_NVM;
	
	}
	
}


CacheEntry*
CerebronPredictor::checkLazyMigration(CacheEntry* current ,uint64_t set,bool inNVM, uint64_t index, bool isWrite)
{
	allocDecision des = activationFunction(isWrite , current->address , current->missPC);

	if( inNVM == false && des == ALLOCATE_IN_NVM)
	{
		//cl is SRAM , check whether we should migrate to NVM 
		if(simu_parameters.enableDatasetSpilling && m_isNVMbusy[set])
		{
			//reportSpilling(rap_current, current->address, false ,  inNVM);
			//stats_busyness_migrate_change++; //If NVM tab is busy , don't migrate
		}
		else
		{
			//Trigger Migration
			int id_assocNVM = evictPolicy(set, true);//Select LRU candidate from NVM cache
			//DPRINTF("DBAMBPredictor:: Migration Triggered from SRAM, index %ld, id_assoc %d \n" , index, id_assoc);

			CacheEntry* replaced_entry_NVM = m_tableNVM[set][id_assocNVM];
					
			/** Migration incurs one read and one extra write */ 
			replaced_entry_NVM->nbWrite++;
			current->nbRead++;
		
			/* Record the write error migration */ 
			Predictor::migrationRecording();
		
			reportMigration(current, false , isWrite);
		
			m_cache->triggerMigration(set, index , id_assocNVM , false);
			if(!m_isWarmup)
				stats_nbMigrations[true]++;


			current = replaced_entry_NVM;
		}

	}
	else if( des == ALLOCATE_IN_SRAM && inNVM == true)
	{
		//cl is in NVM , check whether we should migrate to SRAM 
			
		//If SRAM tab is busy , don't migrate
		if(simu_parameters.enableDatasetSpilling && m_isSRAMbusy[set])
		{
			//reportSpilling(rap_current, current->address, false ,  inNVM);
			//stats_busyness_migrate_change++; //If NVM tab is busy , don't migrate
		}
		else 
		{		
			int id_assocSRAM = evictPolicy(set, false);
		
			CacheEntry* replaced_entry_SRAM = m_tableSRAM[set][id_assocSRAM];

			/** Migration incurs one read and one extra write */ 
			replaced_entry_SRAM->nbWrite++;
			current->nbRead++;
			reportMigration( current, true , isWrite);
	
			m_cache->triggerMigration(set, id_assocSRAM , index , true);
		
			if(!m_isWarmup)
				stats_nbMigrations[false]++;

			current = replaced_entry_SRAM;		
		}

	}
	return current;
}


/*
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
*/


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
	out << entete << ":CerebronPredictor:TotalMigration\t" << stats_nbMigrations[true] + stats_nbMigrations[false] << endl;
	out << entete << ":CerebronPredictor:MigrationsFromSRAM\t" << stats_nbMigrations[true] << endl;
	out << entete << ":CerebronPredictor:MigrationsFromNVM\t" << stats_nbMigrations[false] << endl;
	
	
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
	string h = simu_parameters.Cerebron_enableMigration ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:EnableMigration\t" << h << endl;
	
	out << entete << ":Cerebron:CostValueThreshold\t" << simu_parameters.PHC_cost_threshold  << endl;
	out << entete << ":Cerebron:AllocationThreshold\t" << simu_parameters.perceptron_allocation_threshold  << endl;
	out << entete << ":Cerebron:ActivationFunction\t" << simu_parameters.Cerebron_activation_function  << endl;
	
	string b = simu_parameters.Cerebron_independantLearning ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:IndependantLearning\t" << b  << endl;

	string g = simu_parameters.Cerebron_resetEnergyValues ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:ResetEnergyValeus\t" << g  << endl;
	
	string c = simu_parameters.Cerebron_fastlearning ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:FastLearning\t" << c << endl;
	
	string f = simu_parameters.Cerebron_lowConfidence ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:LowConfidence\t" << c << endl;
	
	string e = simu_parameters.Cerebron_separateLearning ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:SeparateLearning\t" << e << endl;
	
	string d = simu_parameters.Cerebron_RDmodel ? "TRUE" : "FALSE";
	out << entete << ":Cerebron:UsingRDModel\t" << d << endl;

	out << entete << ":Cerebron:decrementValue\t" << simu_parameters.Cerebron_decrement_value << endl;

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

	CacheEntry* entry = NULL;
	for(int i = 0 ; i < m_nb_set ; i++)
	{
		for(int j = 0 ; j < m_nbNVMways ; j++)
		{
			entry = m_tableNVM[i][j];		

			if(!entry->isValid)
				continue;
				
			allocDecision des = getDecision(entry);
			if(des != ALLOCATE_IN_NVM)				
				stats_wrong_alloc++;
			else
				stats_good_alloc++;
		}
		for(int j = 0 ; j < m_nbSRAMways ; j++)
		{
			entry = m_tableSRAM[i][j];		

			if(!entry->isValid)
				continue;

			allocDecision des = getDecision(entry);
			if(des != ALLOCATE_IN_SRAM)				
				stats_wrong_alloc++;
			else
				stats_good_alloc++;
		}
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
//	debug_file << "classifyRD, cpt_time =  " << cpt_time << ", set = " << set << " col = " << index << ", ref_rd = " << ref_rd << endl;
	for(unsigned i = 0 ; i < line.size() ; i ++)
	{
//		debug_file << "\tPolicy[" << i << "] = " << line[i]->policyInfo << endl;
		if(line[i]->policyInfo > ref_rd && line[i]->isValid)
			position++;
	}	
//	debug_file << "Position = " << position << endl;

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
CerebronPredictor::reportAccess(Access element, CacheEntry* current, int set, bool inNVM, string entete, string reuse_class)
{
	if(!simu_parameters.printDebug)
		return;

	string cl_location = inNVM ? "NVM" : "SRAM";
	string is_learning = current->isLearning_policy ? "LearningPolicy" : "Regular";
	is_learning += current->isLearning_weight ? "+LearningWeight" : "";
	is_learning ="";
	string access_type = element.isWrite() ? "WRITE" : "READ";
	
	string is_error= "";
	if(element.isSRAMerror)
		is_error = ", is an SRAM error";

	string des = "[" , confidence = "[", hashes = "[";	
	int total = 0;
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , current->address , current->missPC);		
		confidence += to_string(m_features[i]->getConfidence(hash, true)) + " , ";
		des += string(str_allocDecision[m_features[i]->getAllocDecision(hash, element.isWrite())]) + " , " ;
		hashes += to_string(hash) + " , ";
		
		int conf = m_features[i]->getConfidence(hash, true);
		allocDecision a = m_features[i]->getAllocDecision(hash, element.isWrite());

		if( a == ALLOCATE_IN_SRAM)
			total += (-1) * conf;
		else
			total += conf; 
	}
	allocDecision final_des = convertToAllocDecision(total , element.isWrite());
	int first_hash = hashing_function1( m_features_hash[0] , current->address , current->missPC);
	
	debug_file << entete << ":Dataset n°" << first_hash << ": "
	<< str_allocDecision[final_des ] << "," \
	<< access_type << " on " << is_learning << " Cl 0x" << std::hex << current->address \
	<< std::dec << " allocated in " << cl_location<< ", Reuse=" << reuse_class \
	<< is_error << ", Features Hashes=" << hashes << "] , " \
        << "Des=" << des << "] , Confidence=" << confidence << "]";
        
	if( simu_parameters.Cerebron_RDmodel)
	{
		bool hitSRAM = false, hitNVM = false;
		if( !inNVM && entete == "UPDATE")
			hitSRAM = true;
		else
			hitSRAM = getHitPerDataArray( current->address , set , false);
				
	
		if( inNVM && entete == "UPDATE")
			hitNVM = true;
		else 
			hitNVM = getHitPerDataArray( current->address , set , true);
			
		string hitSRAM_str = hitSRAM ? "TRUE" : "FALSE";
		string hitNVM_str = hitNVM ? "TRUE" : "FALSE";
		
		
		debug_file << ", E_SRAM = " << current->e_sram << " , E_NVM = " << current->e_nvm\
			 << ", HitSRAM=" << hitSRAM_str << ", HitNVM=" << hitNVM_str;	
	}
	else
	{
		debug_file << ", cost = " << current->cost_value;	
	}
	
	debug_file << endl;	
}

void
CerebronPredictor::reportMigration(CacheEntry* current, bool fromNVM, bool isWrite)
{
	
	if(!simu_parameters.printDebug)
		return;	

	string cl_location = fromNVM ? "from NVM" : "from SRAM";

	string des = "[" , confidence = "[", hashes = "[";	
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , current->address , current->missPC);		
		confidence += to_string(m_features[i]->getConfidence(hash, true)) + " , ";
		des += string(str_allocDecision[m_features[i]->getAllocDecision(hash, isWrite)]) + " , " ;
		hashes += to_string(hash) + " , ";
		
	}
	
	int first_hash = hashing_function1( m_features_hash[0] , current->address , current->missPC);
	
	debug_file << "Migration:Dataset n°" << first_hash << ", Migration " << cl_location \
	<< " Cl 0x" << std::hex << current->address << std::dec << ", Features Hashes=" << hashes \
	<< "] , Des=" << des << "] , Confidence="  << confidence << "]" << endl;

}


params_hash
CerebronPredictor::parseFeatureName(string feature_name)
{
	params_hash result;

	vector<string> split_line = split(feature_name , '_');
	result.index = split_line[0];
	result.nbBlock = atoi(split_line[1].c_str());
	
	if(split_line.size() == 3)
		result.xorWithPC = split_line[2] == "1" ? true : false;
	else
		result.xorWithPC = false;
		
	return result;
}


