#ifndef _RAP_PREDICTOR_HH_
#define _RAP_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

#define RAP_OUTPUT_FILE "rap_predictor.out"
#define RAP_OUTPUT_FILE1 "rap_predictor1.out"
#define RAP_TEST_OUTPUT_DATASETS "rap_test_dataset.out"

#define RAP_TABLE_ASSOC 128
#define RAP_TABLE_SET 128

#define RAP_SRAM_ASSOC 4
#define RAP_NVM_ASSOC 12 



class Predictor;
class HybridCache;




class Window_entry
{
	public: 
		Window_entry()
		{
			rd = RD_NOT_ACCURATE;
			doneInNVM = false;
			isWrite = false;
			isBypass = false;
		}
		
		RD_TYPE rd;
		bool doneInNVM;	
		bool isWrite;
		bool isBypass;
};


class RAPEntry
{
	public: 
		RAPEntry() { initEntry(); isValid = false; };
		
		void initEntry() {
		 	m_pc = -1;
		 	
		 	des = ALLOCATE_PREEMPTIVELY;
			state_rw = RW_NOT_ACCURATE;
			state_rd = RD_NOT_ACCURATE;
		 					
		 	policyInfo = 0;
			cptBypassLearning = 0;
			nbAccess = 0;
			isValid = true;
			doMigration = false;
			
			nbKeepCurrentState = 0;
			nbKeepState = 0;
			nbSwitchState = 0;
			cptWindow = 0;
			
			dead_counter = 0;
			/*
			rd_history.clear();
			rw_history.clear();
			*/
		 };
		void resetWindow()
		{
			window.clear();
			cptWindow = 0;
		}
		
		bool isValid;
		
		/* PC of the dataset */
		uint64_t m_pc; 
		
		allocDecision des;
		
		/* Do migration when des != cl location */
		bool doMigration;

		/* Replacement info bits */ 
		int policyInfo;

		/* Those parameters are static, they do not change during exec */ 
		int set,assoc;

		int cptBypassLearning;
		
		std::vector<Window_entry> window;
		int cptWindow;
		
		int nbAccess;
		int nbKeepState;
		int nbSwitchState;
		int nbKeepCurrentState;
		
		RW_TYPE state_rw;
		RD_TYPE state_rd;

		std::vector<HistoEntry> stats_history;
		
		int dead_counter;
};


class RAPReplacementPolicy {

	public :
		RAPReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> >& rap_entries);
		void updatePolicy(uint64_t set, uint64_t assoc);
		void insertionPolicy(uint64_t set, uint64_t assoc) { updatePolicy(set,assoc);}
		int evictPolicy(int set);
	private : 
		std::vector<std::vector<RAPEntry*> >& m_rap_entries;
		uint64_t m_cpt;
		unsigned m_nb_set;
		unsigned m_assoc;
		
};



class RAPPredictor : public Predictor {

	public :
		RAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~RAPPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t assoc, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t assoc, bool inNVM, Access element);
		void updateWindow(RAPEntry* rap_current, Window_entry entry);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		
		void printStats(std::ostream& out);
		void printConfig(std::ostream& out);
		void openNewTimeFrame();
		void finishSimu();
		void dumpDataset(RAPEntry* entry);		
		
		CacheEntry* getEntry(int set, int assoc, bool inNVM);
		RAPEntry* lookup(uint64_t pc);
		uint64_t indexFunction(uint64_t pc);
		void endWindows(RAPEntry* rap_current);

		RD_TYPE computeRd(uint64_t set, uint64_t index, bool inNVM);
		RD_TYPE convertRD(int rd);
		
		
		void determineStatus(RAPEntry* entry);
		allocDecision convertState(RAPEntry* rap_current);
		
		void checkMigration(RAPEntry* rap_current, CacheEntry* current, int set, bool inNVM, int assoc);
		
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

