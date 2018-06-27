#ifndef __DBAMP__PREDICTOR__HH__
#define __DBAMP__PREDICTOR__HH__

#include <vector>
#include <map>
#include <list>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

#define RAP_OUTPUT_FILE "rap_predictor.out"
#define RAP_OUTPUT_FILE1 "rap_predictor1.out"
#define RAP_TEST_OUTPUT_DATASETS "rap_test_dataset.out"
#define RAP_MIGRATION_STATS "rap_migration_stats.out"
#define RAP_DATASET_FIRSTALLOC "rap_firstAlloc_dataset.out"

#define INDEX_READ 0
#define INDEX_WRITE 1

#define FROMNVM 0
#define FROMSRAM 1

class Predictor;
class HybridCache;


class DHPEntry
{
	public: 
		DHPEntry() { initEntry( Access()); isValid = false; };
		
		void initEntry(Access element) {
		 	cpts =  std::vector<int>(NUM_RW_TYPE,0);
		 	lastWrite = 0;
		 	signature = 0;
		 	des = ALLOCATE_PREEMPTIVELY;
		 	
		 	policyInfo = 0;
			cptBypassLearning = 0;
			rd_history.clear();
			rw_history.clear();
			nbAccess = 0;
			isValid = true;
			
			state_rw = RW_NOT_ACCURATE;
			state_rd = RD_NOT_ACCURATE;
			
			nbKeepCurrentState = 0;
			nbKeepState = 0;
			nbSwitchState = 0;
			cptWindow = 0;
			
			dead_counter = 0;
		 };
		/* Saturation counters for the classes of cl*/ 
		std::vector<int> cpts;

		int id;
		int lastWrite;
		
		/* Signature of the dataset */
		uint64_t signature; 
		
		allocDecision des;

		/* Replacement info bits */ 
		int policyInfo;

		/* Those parameters are static, they do not change during exec */ 
		int index,assoc;

		int cptBypassLearning;
		
		std::vector<RD_TYPE> rd_history;
		std::vector<bool> rw_history;
		
		int nbAccess;
		int cptWindow;
		
		bool isValid;
		
		int nbKeepState;
		int nbSwitchState;
		int nbKeepCurrentState;
		
		RW_TYPE state_rw;
		RD_TYPE state_rd;

		std::vector<HistoEntry> stats_history;
		
		int dead_counter;
};


class DBAMBReplacementPolicy{
	
	public : 
		DBAMBReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<DHPEntry*> >& rap_entries ) : m_rap_entries(rap_entries),\
											m_nb_set(nbSet) , m_assoc(nbAssoc) {};
		virtual void updatePolicy(uint64_t set, uint64_t index) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index) = 0;
		virtual int evictPolicy(int set) = 0;
		
	protected : 
		std::vector<std::vector<DHPEntry*> >& m_rap_entries;
		int m_cpt;
		unsigned m_nb_set;
		unsigned m_assoc;
};


class DBAMBLRUPolicy : public DBAMBReplacementPolicy {

	public :
		DBAMBLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<DHPEntry*> >& rap_entries);
		void updatePolicy(uint64_t set, uint64_t index);
		void insertionPolicy(uint64_t set, uint64_t index) { updatePolicy(set,index);}
		int evictPolicy(int set);
	private : 
		uint64_t m_cpt;
		
};


class DBAMBPredictor : public Predictor {

	public :
		DBAMBPredictor(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~DBAMBPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);
		void openNewTimeFrame();
		void finishSimu();
		DHPEntry* lookup(uint64_t pc);
		uint64_t indexFunction(uint64_t pc);

		int computeRd(uint64_t set, uint64_t index, bool inNVM);
		RD_TYPE convertRD(int rd);
		RD_TYPE evaluateRd(std::vector<int> reuse_distances);
		
		void determineStatus(DHPEntry* entry);
		allocDecision convertState(DHPEntry* rap_current);

		void updateWindow(DHPEntry* rap_current);
		
		CacheEntry* checkLazyMigration(DHPEntry* rap_current , CacheEntry* current ,uint64_t set,bool inNVM , uint64_t index, bool isWrite);

		void reportAccess(DHPEntry* rap_current, Access element, CacheEntry* current, bool inNVM, std::string entete, std::string reuse_class);
		void reportMigration(DHPEntry* rap_current, CacheEntry* current, bool fromNVM);
		void reportSpilling(DHPEntry* rap_current, uint64_t addr, bool isAlloc,  bool inNVM);
		
		uint64_t hashingSignature(Access element);
		uint64_t hashingFunction(Access element);

	protected : 
		uint64_t m_cpt;
		int learningTHcpt;
		
		/* cost function optimization parameter*/ 
		std::string m_optTarget;
		CostFunctionParameters m_costParameters;

		/* DHP Table Handlers	*/
		std::vector< std::vector<DHPEntry*> > m_DHPTable;
		DBAMBReplacementPolicy* m_rap_policy;
		
		/* Used if a file with the registered first dataset allocation exists */ 
		std::map<uint64_t, std::vector<allocDecision> > m_firstAlloc_datasets;
		
		unsigned m_RAP_assoc;
		unsigned m_RAP_sets; 

		std::vector<int> m_hashingBytes;
	
		/* Stats */
		
		unsigned stats_RAP_miss;
		unsigned stats_RAP_hits;

		unsigned stats_NVM_medium_pred_errors;
		unsigned stats_NVM_medium_pred;
		
		uint64_t stats_busyness_alloc_change,stats_busyness_migrate_change;
				
//		std::vector< std::vector< std::vector<int> > > stats_switchDecision;		
		std::vector<double> stats_nbSwitchDst;
		std::vector< std::vector<int> > stats_ClassErrors;

		/* Lazy Migration opt */ 
		std::vector< std::vector<int> > stats_nbMigrationsFromNVM;
		std::vector< std::list<std::pair<uint64_t,uint64_t> > > stats_history_SRAM;

		uint64_t stats_error_wrongallocation, stats_error_learning, stats_error_wrongpolicy;
		uint64_t stats_error_SRAMwrongallocation, stats_error_SRAMlearning, stats_error_SRAMwrongpolicy;
		
};


#endif

