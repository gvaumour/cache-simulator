#ifndef __PERCEPTRON__PREDICTOR__HH__
#define __PERCEPTRON__PREDICTOR__HH__

#include <vector>
#include <map>
#include <list>
#include <ostream>

#include "FeatureTable.hh"
#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

#define FILE_OUTPUT_PERCEPTRON "output_perceptron.out"
#define FILE_DUMP_PERCEPTRON "dump_perceptron.out"
class Predictor;
class HybridCache;


typedef int (*hashing_function)(uint64_t , uint64_t , uint64_t); 

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
		void openNewTimeFrame();
		void finishSimu();
		RD_TYPE classifyRD(int set , int index , bool inNVM);
		void setPrediction(CacheEntry* current);
		CacheEntry* get_entry(uint64_t set , uint64_t index , bool inNVM);
				
	protected : 

		/* FeatureTable Handlers */
		std::vector<FeatureTable*> m_features;
		std::vector<hashing_function> m_features_hash;
		std::vector<std::string> m_criterias_names;
		int m_tableSize;

		uint64_t m_cpt;		
		
		/* Stats */ 
		std::vector<uint64_t> stats_nbBPrequests;
		std::vector<uint64_t> stats_nbDeadLine;
		std::vector<int> stats_missCounter;
		uint64_t stats_nbMiss, stats_nbHits;
};


int hashingAddr_MSB(uint64_t addr , uint64_t missPC , uint64_t currentPC);
int hashingAddr_LSB(uint64_t addr , uint64_t missPC , uint64_t currentPC);
int hashingAddr_LSB1(uint64_t addr , uint64_t missPC , uint64_t currentPC);

int hashingMissPC_MSB(uint64_t addr , uint64_t missPC , uint64_t currentPC);
int hashingMissPC_LSB(uint64_t addr , uint64_t missPC , uint64_t currentPC);
int hashingMissPC_LSB1(uint64_t addr , uint64_t missPC , uint64_t currentPC);

int hashing_MissCounter(uint64_t addr, uint64_t missPC, uint64_t currentPC);
int hashing_MissCounter1(uint64_t addr, uint64_t missPC, uint64_t currentPC);

int hashing_currentPC(uint64_t addr, uint64_t missPC, uint64_t currentPC);

#endif

