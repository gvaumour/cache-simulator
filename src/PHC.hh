#ifndef __PHC__PREDICTOR__HH__
#define __PHC__PREDICTOR__HH__

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

#define FILE_OUTPUT_PHC "output_PHC.out"


class Predictor;
class HybridCache;

class PHCPredictor : public Predictor {

	public :
		PHCPredictor(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~PHCPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM);
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);
		void openNewTimeFrame();
		void finishSimu();
		void drawFeatureMaps();

		
		RD_TYPE classifyRD(int set , int index , bool inNVM);
		CacheEntry* get_entry(uint64_t set , uint64_t index , bool inNVM);

		void update_globalPChistory(uint64_t pc);
		void load_callee_file();
		void initFeatures();
	
		static std::deque<uint64_t> m_global_PChistory;
		static std::deque<uint64_t> m_callee_PChistory;

				
	protected : 

		/* FeatureTable Handlers */
		std::vector<FeatureTable*> m_features;
		std::vector<hashing_function> m_features_hash;
		std::vector<std::string> m_criterias_names;

		std::vector< std::vector<int> > m_costAccess;

		int m_tableSize;
		uint64_t m_cpt;		

		bool m_need_callee_file;
		std::map<uint64_t,int> m_callee_map;
		
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
		
		uint64_t stats_cptLearningSRAMerror, stats_cptSRAMerror;
			
		uint64_t stats_prediction_confident, stats_prediction_preemptive;
		std::vector< std::vector< std::vector<int> > > stats_histo_value;
		
		std::vector<std::vector<int> > stats_local_error;
		std::vector< std::vector<int> > stats_global_error;
		std::vector< std::vector<int> > stats_access_class;
};




#endif

