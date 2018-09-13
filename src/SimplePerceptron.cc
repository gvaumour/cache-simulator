#include <cmath>
#include <vector>
#include <map>
#include <ostream>

#include "SimplePerceptron.hh"

using namespace std;


ofstream debug_file_simple_perceptron;

SimplePerceptronPredictor::SimplePerceptronPredictor(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) :\
	Predictor(id, nbAssoc , nbSet , nbNVMways , SRAMtable , NVMtable , cache)
{	
	m_tableSize = simu_parameters.perceptron_table_size;

	m_feature_names = simu_parameters.simple_perceptron_features;
	
	initFeatures();
	
	for(auto p : simu_parameters.simple_perceptron_features)
		cout << p << ",";
	cout << endl;
	
	m_costParameters = simu_parameters.optTarget;
	
	m_cpt = 0;
	
	if(simu_parameters.printDebug)
		debug_file_simple_perceptron.open(SIMPLE_PERCEPTRON_DEBUG_FILE);	
	
	stats_access_class = vector<vector<int> >(NUM_RD_TYPE ,  vector<int>(2, 0));
	stats_nbMigrations = vector<int>(2,0);
}


SimplePerceptronPredictor::~SimplePerceptronPredictor()
{
	for(auto feature : m_features)
		delete feature;
}

void
SimplePerceptronPredictor::initFeatures()
{
	vector<string>* criterias_name = &m_feature_names;
	vector<params_hash>* features_hash = &m_features_hash;
	vector<SimpleFeatureTable*>* features = &m_features;		

	for(auto feature : *criterias_name)
	{
		features->push_back( new SimpleFeatureTable(m_tableSize, feature, false));
		features_hash->push_back( parseFeatureName(feature) );
	}	
}


allocDecision
SimplePerceptronPredictor::allocateInNVM(uint64_t set, Access element)
{	
	Predictor::update_globalPChistory(element.m_pc);
	
	if(isHitInBPtags(element.block_addr , set))
		element.m_pc = getMissPCfromBPtags(element.block_addr , set);
	
	/*
	if( hitInSRAMMissingTags( element.block_addr, set) )
		element.m_pc = missingTagMissPC( element.block_addr , set);
	*/
	
	// All the set is a learning/sampled set independantly of its way or SRAM/NVM alloc
	//bool isLearning = m_tableSRAM[set][0]->isLearning; 
	allocDecision des =  activationFunction(element);
	return des;
}


void
SimplePerceptronPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* entry = inNVM ? m_tableNVM[set][index] : m_tableSRAM[set][index];
	entry->missPC = element.m_pc;
		
	if(isHitInBPtags(element.block_addr , set))
	{
		entry->missPC = getMissPCfromBPtags(element.block_addr , set);
		correctBPerror(entry , element , set );
			/*
		debug_file_simple_perceptron << "Insertion + detected SRAM error on line 0x" << std::hex << element.block_addr \
			 << " , Retified MissPC from 0x" << element.m_pc << " to " << entry->missPC << endl;
		*/
	}

	entry->policyInfo = m_cpt++;

	if(entry->isLearning)
	{		
		updateCostModel(entry, entry->address , set , element.isWrite());		

		if(simu_parameters.simple_perceptron_fastLearning)
			doLearning(entry);

		setPrediction(entry , element);	
	}	
	reportAccess( element, entry , set , inNVM, string("INSERTION") );
	
//	stats_access_class[rd][element.isWrite()]++;
	Predictor::insertionPolicy(set , index , inNVM , element);
}


void
SimplePerceptronPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest)
{
	CacheEntry* entry = inNVM ? m_tableNVM[set][index] : m_tableSRAM[set][index];
//	RD_TYPE rd = classifyRD( set , index , inNVM );
//	stats_access_class[rd][element.isWrite()]++;
	
	entry->policyInfo = m_cpt++;
	Predictor::update_globalPChistory(element.m_pc);

	if(entry->isLearning)
	{
		updateCostModel(entry, entry->address , set , element.isWrite());
		if(simu_parameters.simple_perceptron_fastLearning)
		{
			doLearning(entry); //Update the dataset confidence
			setPrediction(entry, element); //Set the prediction ready for the next access
		}
	}

	if(simu_parameters.simple_perceptron_enableMigration)
	{
		assert(entry->isValid);
		entry = checkLazyMigration(entry , set , inNVM , index, element.isWrite());		
	}


	reportAccess( element, entry , set , inNVM, string("UPDATE") );

	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}

int SimplePerceptronPredictor::evictPolicy(int set, bool inNVM)
{
	int assoc_victim = -1;

	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);

	CacheEntry* entry = inNVM ? m_tableNVM[set][assoc_victim] : m_tableSRAM[set][assoc_victim];

	if(entry->isValid)
	{	
		reportEviction(entry, inNVM);
			
		if(entry->isLearning && !simu_parameters.simple_perceptron_fastLearning)
			doLearning(entry);
	}
	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}


void
SimplePerceptronPredictor::evictRecording( int id_set , int id_assoc , bool inNVM)
{ 
	Predictor::evictRecording(id_set, id_assoc, inNVM);
}

void 
SimplePerceptronPredictor::doLearning(CacheEntry* entry)
{
	allocDecision correct_des =  entry->e_sram > entry->e_nvm ? ALLOCATE_IN_NVM : ALLOCATE_IN_SRAM;

	if(simu_parameters.simple_perceptron_independantLearning)
	{
	
		for(unsigned i = 0 ; i < m_features.size() ; i++)
		{
			int hash = entry->simple_perceptron_hash[i];
			//if(entry->simple_perceptron_pred[i] != correct_des)
			//{
				if( correct_des == ALLOCATE_IN_NVM)
					m_features[i]->increaseCounter(hash);
				else
					m_features[i]->decreaseCounter(hash);
			//}
			/*
			else
				m_features[i]->increaseCounter(hash);
			*/
		}		
	}
	else
	{
		//allocDecision des = entry->simple_perceptron_pred[0];
		//if( des != correct_des)
		//{
			if( correct_des == ALLOCATE_IN_NVM)
			{
				for(unsigned i = 0 ; i < m_features.size() ; i++)
				{
					int hash = entry->simple_perceptron_hash[i];
					m_features[i]->increaseCounter(hash);
				}

			}
			else
			{			
				for(unsigned i = 0 ; i < m_features.size() ; i++)
				{
					int hash = entry->simple_perceptron_hash[i];
					m_features[i]->decreaseCounter(hash);
				}
			}
		//}
		/*
		else
		{
			for(unsigned i = 0 ; i < m_features.size() ; i++)
			{
				int hash = entry->simple_perceptron_hash[i];
				m_features[i]->increaseCounter(hash);
			}		
		}*/
	}
}

allocDecision
SimplePerceptronPredictor::activationFunction(Access element)
{

	int yout = 0;
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , element.block_addr , element.m_pc);
		int pred = m_features[i]->getCounter(hash);
		yout += pred;
	}
	return getDecision(yout , element.isWrite());
}

allocDecision
SimplePerceptronPredictor::getDecision(int counter, bool isWrite)
{
	allocDecision des = ALLOCATE_IN_SRAM;
	/* If we are not sure enough , we go preemptive */ 

	if( abs(counter) > simu_parameters.simple_perceptron_learningTreshold)
	{

		if(counter > 0)
			des = ALLOCATE_IN_NVM;
		else
			des = ALLOCATE_IN_SRAM;
	}
	else
	{
		if(isWrite)
			des = ALLOCATE_IN_SRAM;
		else
			des = ALLOCATE_IN_NVM;
	}

	return des;
}


void 
SimplePerceptronPredictor::setPrediction(CacheEntry* entry, Access element)
{
	int sum_pred = 0;
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = hashing_function1(m_features_hash[i] , element.block_addr , entry->missPC);
		int counter = m_features[i]->getCounter(hash);
		sum_pred += counter;
		entry->simple_perceptron_hash[i] = hash;
		if(simu_parameters.simple_perceptron_independantLearning)
			entry->simple_perceptron_pred[i] = getDecision(counter , element.isWrite());
	}
	
	if(!simu_parameters.simple_perceptron_independantLearning)
		entry->simple_perceptron_pred[0] = getDecision(sum_pred , element.isWrite());

}

CacheEntry*
SimplePerceptronPredictor::checkLazyMigration(CacheEntry* current ,uint64_t set,bool inNVM, uint64_t index, bool isWrite)
{

	int yout = 0;
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , current->address , current->missPC );
		int pred = m_features[i]->getCounter(hash);
		yout += pred;
	}
	
	if ( abs(yout) < simu_parameters.simple_perceptron_learningTreshold)
		return current;

	allocDecision des = yout > 0 ? ALLOCATE_IN_NVM : ALLOCATE_IN_SRAM;

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
		
void 
SimplePerceptronPredictor::updateCostModel(CacheEntry* entry , uint64_t block_addr , int set, bool isWrite)
{
	bool hitSRAM = false, hitNVM = false;
	hitNVM = getHitPerDataArray( block_addr , set , true);
	hitSRAM = getHitPerDataArray( block_addr , set , false);
	
	entry->e_sram += m_costParameters.costSRAM[isWrite];
	entry->e_sram +=  !hitSRAM ? m_costParameters.costDRAM[isWrite] : 0;

	entry->e_nvm += m_costParameters.costNVM[isWrite];
	entry->e_nvm += !hitNVM ? m_costParameters.costDRAM[isWrite] : 0;
}

void
SimplePerceptronPredictor::correctBPerror(CacheEntry* entry , Access element , int set)
{
	bool wasInNVM = getAllocSitefromBPtags(element.block_addr, set);

	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , entry->address , entry->missPC);		
		
		if( wasInNVM )
		{
			m_features[i]->decreaseCounter(hash);		
			m_features[i]->decreaseCounter(hash);		
		}
		else
		{
			m_features[i]->increaseCounter(hash);		
			m_features[i]->increaseCounter(hash);		
		}
	}
}


/*
void
SimplePerceptronPredictor::update_globalPChistory(uint64_t pc)
{	
 	m_global_PChistory.push_front(pc);
	if( m_global_PChistory.size() == 11)
		m_global_PChistory.pop_back();
}
*/


void 
SimplePerceptronPredictor::printStats(std::ostream& out, std::string entete) 
{ 
	out << entete << ":SimplePerceptronPredictor:SRAMerror\t" << stats_cptSRAMerror << endl;
	if(simu_parameters.simple_perceptron_enableMigration)
	{
		out << entete << ":SimplePerceptronPredictor:TotalMigration\t" << stats_nbMigrations[true] + stats_nbMigrations[false] << endl;
		out << entete << ":SimplePerceptronPredictor:MigrationsFromSRAM\t" << stats_nbMigrations[true] << endl;
		out << entete << ":SimplePerceptronPredictor:MigrationsFromNVM\t" << stats_nbMigrations[false] << endl;	
	}
	
	out << entete << ":SimplePerceptronPredictor:AccessClassification" << endl;
	for(unsigned i = 0 ; i < stats_access_class.size() ; i++)
	{
		if(stats_access_class[i][0] != 0 || stats_access_class[i][1] != 1)
		{
			out << entete << ":SimplePerceptronPredictor:Read" <<  str_RD_status[i]	<< "\t" << stats_access_class[i][false] << endl;	
			out << entete << ":SimplePerceptronPredictor:Write" << str_RD_status[i] << "\t" << stats_access_class[i][true] << endl; 		
		}
	}
	Predictor::printStats(out, entete);
}



void SimplePerceptronPredictor::printConfig(std::ostream& out, std::string entete) { 

	out << entete << ":SimplePerceptron:TableSize\t" << m_tableSize << endl; 
		
	
	out << entete << ":SimplePerceptron:Features\t"; 
	for(auto p : m_feature_names)
		out << p << ",";
	out << endl;

	out << entete << ":SimplePerceptron:LearningThreshold\t" << simu_parameters.simple_perceptron_learningTreshold << endl;
	out << entete << ":SimplePerceptron:FastLearning\t" << convertBool(simu_parameters.simple_perceptron_fastLearning) << endl;
	out << entete << ":SimplePerceptron:IndependantLearning\t" << convertBool(simu_parameters.simple_perceptron_independantLearning) << endl;
	out << entete << ":SimplePerceptron:enableMigration\t" << convertBool(simu_parameters.simple_perceptron_enableMigration) << endl;
	
	Predictor::printConfig(out, entete);
}

void
SimplePerceptronPredictor::openNewTimeFrame()
{
	for(auto feature : m_features)
		feature->openNewTimeFrame();	
	
	Predictor::openNewTimeFrame();
}

void 
SimplePerceptronPredictor::finishSimu()
{
	for(auto feature : m_features)
		feature->finishSimu();
	Predictor::finishSimu();
}

/*
RD_TYPE
SimplePerceptronPredictor::classifyRD(int set , int index, bool inNVM)
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
*/

void
SimplePerceptronPredictor::reportAccess(Access element, CacheEntry* current, int set , bool inNVM, string entete)
{
	if(!simu_parameters.printDebug)
		return;
	
	string cl_location = inNVM ? "NVM" : "SRAM";
	string access_type = element.isWrite() ? "WRITE" : "READ";
	
	string is_error= "";
	if( isHitInBPtags(element.block_addr , set) && entete == "INSERTION")
	{
		is_error == "is a BP error";
	}
	//if(element.isSRAMerror)
	//	is_error = "is an SRAM error, ";

	string counters = "[";
	string hashes = "[";	
	
	for(unsigned i = 0 ; i < m_features.size()-1; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , current->address , current->missPC);		
		int counter = m_features[i]->getCounter(hash);
		counters += to_string(counter) + " , ";
		hashes += to_string(hash) + " , ";
	}
	int hash = hashing_function1( m_features_hash[m_features.size()-1] , current->address , current->missPC);		
	counters += to_string(m_features[m_features.size()-1]->getCounter(hash)) + " ]";
	hashes	+= to_string(hash) + " ]";
	
	int first_hash = hashing_function1( m_features_hash[0] , current->address , current->missPC);
	
	debug_file_simple_perceptron << entete << ":Dataset n°" << first_hash << ": " \
	<< access_type << " on Cl 0x" << std::hex << current->address << std::dec << ", E_SRAM =" \
	<< current->e_sram << " , E_NVM =" << current->e_nvm << " allocated in " << cl_location << ", " << is_error << "Hashes=" << hashes \
        << ", Counters=" << counters << endl;	
}

void
SimplePerceptronPredictor::reportEviction(CacheEntry* entry, bool inNVM)
{
	if(!simu_parameters.printDebug)
		return;
		
	string cl_location = inNVM ? "NVM" : "SRAM";
	string counters = "[";
	string hashes = "[";	
	
	for(unsigned i = 0 ; i < m_features.size()-1; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , entry->address , entry->missPC);		
		int counter = m_features[i]->getCounter(hash);
		counters += to_string(counter) + " , ";
		hashes += to_string(hash) + " , ";
	}
	
	int hash = hashing_function1( m_features_hash[m_features.size()-1] , entry->address , entry->missPC);		
	counters += to_string(m_features[m_features.size()-1]->getCounter(hash)) + " ]";
	hashes	+= to_string(hash) + " ]";
	int first_hash = hashing_function1( m_features_hash[0] , entry->address , entry->missPC);

	debug_file_simple_perceptron << "EVICTION:Dataset n°" << first_hash << ": Cl 0x" << std::hex << entry->address \
	<< std::dec << ", E_SRAM =" << entry->e_sram << " , E_NVM =" << entry->e_nvm << " allocated in " << cl_location << ", Hashes=" << hashes \
        << ", Counters=" << counters << endl;
}

void
SimplePerceptronPredictor::reportMigration(CacheEntry* current, bool fromNVM, bool isWrite)
{
	
	if(!simu_parameters.printDebug)
		return;	

	string cl_location = fromNVM ? "from NVM" : "from SRAM";

	string confidence = "[", hashes = "[";	
	for(unsigned i = 0 ; i < m_features.size() ; i++)
	{
		int hash = hashing_function1( m_features_hash[i] , current->address , current->missPC);		
		confidence += to_string(m_features[i]->getCounter(hash)) + " , ";
		hashes += to_string(hash) + " , ";
		
	}
	
	int first_hash = hashing_function1( m_features_hash[0] , current->address , current->missPC);
	
	debug_file_simple_perceptron << "Migration:Dataset n°" << first_hash << ", Migration " << cl_location \
	<< " Cl 0x" << std::hex << current->address << std::dec << ", Features Hashes=" << hashes \
	<< "] , Counters="  << confidence << "]" << endl;

}


params_hash
SimplePerceptronPredictor::parseFeatureName(string feature_name)
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


