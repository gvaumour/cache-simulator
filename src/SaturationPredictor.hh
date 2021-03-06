#ifndef SATURATION_PREDICTOR_HH_
#define SATURATION_PREDICTOR_HH_

#include <vector>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

class Predictor;
class HybridCache;

/*
	Implementation of the saturation-based predictor 
	"Power and performance of read-write aware Hybrid Caches with non-volatile memories,"
	Xiaoxia Wu, Jian Li, Lixin Zhang, E. Speight and Yuan Xie,
	DATE 2009
*/

class SaturationCounter : public Predictor {

	public :
//		SaturationCounter();
		SaturationCounter(int id, int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		int evictPolicy(int set, bool inNVM);
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);
		void openNewTimeFrame() { };
		void finishSimu() {};

		~SaturationCounter();
		
	protected : 
		int m_cpt;
		int threshold;
		std::vector<int> stats_nbMigrationsFromNVM;
};

#endif
