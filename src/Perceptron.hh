#ifndef __PERCEPTRON__PREDICTOR__HH__
#define __PERCEPTRON__PREDICTOR__HH__

#include <vector>
#include <map>
#include <list>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

class Predictor;
class HybridCache;


typedef int (*hashing_function)(CacheEntry*); 

class CriteriaEntry
{
	public: 
		CriteriaEntry() { initEntry(); isValid = false; };
		
		void initEntry() {
		 	cpts =  std::vector< std::vector<int> >(NUM_RW_TYPE, std::vector<int>(NUM_RD_TYPE, 0));
		 					
			isValid = true;
			cptWindow = 0;			
			dead_counter = 0;
			cptBypassLearning = 0;
			allocation_counter = 0;
			des = ALLOCATE_PREEMPTIVELY;
		 };
		
		void updateDatasetState();
		
		std::vector< std::vector<int> > cpts;

		int cptBypassLearning;		
		int cptWindow;
		
		bool isValid;
		int dead_counter;
		
		int allocation_counter;
		allocDecision des;
};


class CriteriaTable
{

	public: 
		CriteriaTable( int size, int (*hashing)(CacheEntry*) , std::string name);
		~CriteriaTable();

		CriteriaEntry* lookup(CacheEntry*);
		int getAllocationPrediction(int set , Access element);
		void updateEntry(CacheEntry* entry, Access element, RD_TYPE rd);
		
	private:
		int m_size;
		std::string m_name;
		std::vector<CriteriaEntry*> m_table;
		hashing_function m_hash;
};		

class PerceptronPredictor : public Predictor {

	public :
		PerceptronPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~PerceptronPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM);
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);
		void openNewTimeFrame() {};
		void finishSimu() {};
		RD_TYPE classifyRD(int set , int index , bool inNVM);
		
		CacheEntry* get_entry(uint64_t set , uint64_t index , bool inNVM);
				
	protected : 

		/* CriteriaTable Handlers */
		std::vector<CriteriaTable*> m_criterias;
		std::vector<std::string> m_criterias_names;
		int m_tableSize;
		
};


int hashingAddr_MSB(CacheEntry* entry);
int hashingAddr_LSB(CacheEntry* entry);
int hashingPC_MSB(CacheEntry* entry);
int hashingPC_LSB(CacheEntry* entry);

#endif

