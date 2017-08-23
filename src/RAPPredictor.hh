#ifndef _RAP_PREDICTOR_HH_
#define _RAP_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"
#include "../RAP_config.hh"

#define RAP_OUTPUT_FILE "rap_predictor.out"
#define RAP_OUTPUT_FILE1 "rap_predictor1.out"
#define RAP_TEST_OUTPUT_DATASETS "rap_test_dataset.out"

#define SRAM_READ 271.678E-12
#define SRAM_WRITE 257.436E-12
#define NVM_WRITE 652.278E-12
#define NVM_READ 236.828E-12
#define DRAM_READ 4.10241E-9
#define DRAM_WRITE 4.08893E-9

#define RAP_TABLE_ASSOC 128
#define RAP_TABLE_SET 128

#define RAP_SRAM_ASSOC 4
#define RAP_NVM_ASSOC 12 

#define INDEX_READ 0
#define INDEX_WRITE 1

class Predictor;
class HybridCache;


class RAPEntry
{
	public: 
		RAPEntry() { initEntry( Access()); isValid = false; };
		
		void initEntry(Access element) {
		 	lastWrite = 0;
		 	m_pc = -1;
		 	
		 	
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

		int lastWrite;
		
		/* PC of the dataset */
		uint64_t m_pc; 
		
		allocDecision des;

		/* Replacement info bits */ 
		int policyInfo;

		/* Those parameters are static, they do not change during exec */ 
		int index,assoc;

		int cptBypassLearning;
		
		std::vector<int> rd_history;
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
//		std::set<CacheEntry*> dataset_cls;
		
		
		int dead_counter;
};


class RAPReplacementPolicy{
	
	public : 
		RAPReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> >& rap_entries ) : m_rap_entries(rap_entries),\
											m_nb_set(nbSet) , m_assoc(nbAssoc) {};
		virtual void updatePolicy(uint64_t set, uint64_t index) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index) = 0;
		virtual int evictPolicy(int set) = 0;
		
	protected : 
		std::vector<std::vector<RAPEntry*> >& m_rap_entries;
		int m_cpt;
		unsigned m_nb_set;
		unsigned m_assoc;
};


class RAPLRUPolicy : public RAPReplacementPolicy {

	public :
		RAPLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> >& rap_entries);
		void updatePolicy(uint64_t set, uint64_t index);
		void insertionPolicy(uint64_t set, uint64_t index) { updatePolicy(set,index);}
		int evictPolicy(int set);
	private : 
		uint64_t m_cpt;
		
};



class RAPPredictor : public Predictor {

	public :
		RAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~RAPPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void printStats(std::ostream& out);
		void printConfig(std::ostream& out);
		void openNewTimeFrame();
		void finishSimu();
		RAPEntry* lookup(uint64_t pc);
		uint64_t indexFunction(uint64_t pc);

		int computeRd(uint64_t set, uint64_t index, bool inNVM);
		RD_TYPE convertRD(int rd);
		RD_TYPE evaluateRd(std::vector<int> reuse_distances);
		
		void determineStatus(RAPEntry* entry);
		allocDecision convertState(RAPEntry* rap_current);
		void dumpDataset(RAPEntry* entry);		

		void updateWindow(RAPEntry* rap_current);
		
		void checkLazyMigration(RAPEntry* rap_current , CacheEntry* current ,uint64_t set,bool inNVM , uint64_t index);

	protected : 
		uint64_t m_cpt;
		int learningTHcpt;

		/* RAP Table Handlers	*/
		std::vector< std::vector<RAPEntry*> > m_RAPtable;
		RAPReplacementPolicy* m_rap_policy;
		unsigned m_RAP_assoc;
		unsigned m_RAP_sets; 
		
		unsigned stats_RAP_miss;
		unsigned stats_RAP_hits;
				
		/* Stats */
//		std::vector< std::vector< std::vector<int> > > stats_switchDecision;		
		std::vector<double> stats_nbSwitchDst;
		std::vector< std::vector<int> > stats_ClassErrors;

		/* Lazy Migration opt */ 
		std::vector<int> stats_nbMigrationsFromNVM;
};



#endif

