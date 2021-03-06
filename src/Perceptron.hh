#ifndef __PERCEPTRON__PREDICTOR__HH__
#define __PERCEPTRON__PREDICTOR__HH__

#include <vector>
#include <map>
#include <deque>
#include <ostream>

#include "FeatureTable.hh"
#include "FeaturesFunction.hh"

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

#define FILE_OUTPUT_PERCEPTRON "output_perceptron.out"
#define FILE_DUMP_PERCEPTRON "dump_perceptron.out"
class Predictor;
class HybridCache;


class PerceptronPredictor : public Predictor {

	public :
		PerceptronPredictor(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~PerceptronPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM);
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);
		void openNewTimeFrame();
		void finishSimu();

		
		void setBPPrediction(CacheEntry* current);
		void setAllocPrediction(CacheEntry *entry);
		
		RD_TYPE classifyRD(int set , int index , bool inNVM);
		CacheEntry* get_entry(uint64_t set , uint64_t index , bool inNVM);

		void update_globalPChistory(uint64_t pc);
		void load_callee_file();
		void initFeatures(bool isBP);
	
		static std::deque<uint64_t> m_global_PChistory;
		static std::deque<uint64_t> m_callee_PChistory;

				
	protected : 

		/* FeatureTable Handlers */
		std::vector<FeatureTable*> m_Allocation_features;
		std::vector<hashing_function> m_Allocation_features_hash;
		std::vector<std::string> m_Allocation_criterias_names;

		std::vector<FeatureTable*> m_BP_features;
		std::vector<hashing_function> m_BP_features_hash;
		std::vector<std::string> m_BP_criterias_names;

		int m_tableSize;
		uint64_t m_cpt;		

		bool m_need_callee_file;
		std::map<uint64_t,int> m_callee_map;
		
		bool m_enableBP;
		bool m_enableAllocation;
		
		/* Stats */ 
		std::vector<uint64_t> stats_nbBPrequests;
		std::vector<uint64_t> stats_nbDeadLine;
		std::vector<int> stats_missCounter;
		uint64_t stats_nbMiss, stats_nbHits;
		std::vector<double> stats_variances_buffer;
		std::vector<double> stats_variances;

		uint64_t stats_update , stats_update_learning;
		uint64_t stats_allocate , stats_allocate_learning;
				

};




#endif

