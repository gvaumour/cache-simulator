#ifndef __SIMPLEPERCEPTRON__PREDICTOR__HH__
#define __SIMPLEPERCEPTRON__PREDICTOR__HH__

#include <vector>
#include <map>
#include <deque>
#include <ostream>

#include "SimpleFeatureTable.hh"
#include "FeaturesFunction.hh"

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

#define SIMPLE_PERCEPTRON_DEBUG_FILE "simple_perceptron_debug.out"

class Predictor;
class HybridCache;
class SimpleFeatureTable;


class SimplePerceptronPredictor : public Predictor {

	public :
		SimplePerceptronPredictor(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~SimplePerceptronPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM);
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);
		void openNewTimeFrame();
		void drawFeatureMaps();
		void finishSimu();
		
		void reportAccess(Access element, CacheEntry* current, int set , bool inNVM, std::string entete);
		void reportEviction(CacheEntry* entry, bool inNVM);
		void reportMigration(CacheEntry* current, bool fromNVM, bool isWrite);

		allocDecision activationFunction(Access element);
		allocDecision getDecision(int counter, bool isWrite);

		void updateCostModel(CacheEntry* entry , uint64_t block_addr , int set, bool isWrite);
		void doLearning(CacheEntry* entry);
		void setPrediction(CacheEntry* entry, Access element);
		void correctBPerror(CacheEntry* entry , Access element , int set );
		
		CacheEntry* checkLazyMigration(CacheEntry* current ,uint64_t set,bool inNVM, uint64_t index, bool isWrite);

		params_hash parseFeatureName(std::string feature_name);
		
		void initFeatures();

				
	protected : 
		
		/* FeatureTable Handlers */
		std::vector<SimpleFeatureTable*> m_features;
		std::vector<params_hash> m_features_hash;
		std::vector<std::string> m_feature_names;
		
		int m_tableSize;
		uint64_t m_cpt;
		CostFunctionParameters m_costParameters;
		
		/* Stats */ 
		uint64_t stats_nbMiss, stats_nbHits;
		uint64_t stats_cptSRAMerror;
		uint64_t stats_good_alloc, stats_wrong_alloc;
		std::vector< std::vector<int> > stats_access_class;	
		std::vector<int> stats_nbMigrations;	

};




#endif

