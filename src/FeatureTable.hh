#ifndef __FEATURE__TABLE__HH__
#define __FEATURE__TABLE__HH__

#include <vector>
#include <string>

#include "Cache.hh"
#include "common.hh"

class FeatureEntry
{
	public: 
		FeatureEntry() { initEntry(); isValid = false; };
		
		void initEntry() {
			if(simu_parameters.Cerebron_RDmodel)
			{
			 	SRAM_seq = std::vector< std::vector<int> >(2, std::vector<int>(2, 0));	
			 	NVM_seq = std::vector< std::vector<int> >(2, std::vector<int>(2, 0));				
			}
			else
			 	cpts =  std::vector< std::vector<int> >(NUM_RW_TYPE, std::vector<int>(NUM_RD_TYPE, 0));

			isValid = true;
			cptWindow = 0;

			weight = simu_parameters.perceptron_counter_size/2;
			des = ALLOCATE_PREEMPTIVELY;
		 };
		
		void updateDatasetState();
		
		std::vector< std::vector<int> > cpts;
		std::vector< std::vector<int> > SRAM_seq;
		std::vector< std::vector<int> > NVM_seq;

		int cptWindow;
		
		bool isValid;

		int weight;				
		int allocation_counter;
//		int bypass_counter;
		allocDecision des;
};


class FeatureTable
{

	public: 
		FeatureTable( int size, std::string name , bool isBP);
		~FeatureTable();

		FeatureEntry* lookup(int index);
//		int getAllocationPrediction(int index);
//		int getBypassPrediction(int index);

		int getConfidence(int index, bool isLegal);

		void openNewTimeFrame();
		void finishSimu();
		void recordEvict(int index , bool hasBeenReused);
		void recordAccess(int index, bool isWrite, RD_TYPE rd);
		void recordAccess(int index, bool isWrite, bool isHitSRAM, bool isHitNVM);

//		void decreaseBPConfidence(int index);
//		void increaseBPConfidence(int index);

		void decreaseConfidence(int index);
		void increaseConfidence(int index);
		
		void registerError(int index , bool isError);

		void decreaseAlloc(int index);
		void increaseAlloc(int index);
		int getAllocationPrediction(int index, bool isLegal);
		allocDecision getAllocDecision(int index, bool isWrite);
		
//		std::vector< std::vector<int> > resize_image(std::vector< std::vector<int> >& img);
		FeatureEntry* getEntry(int hash){ return m_table[hash];};
		
	private:
		int m_size;
		std::string m_name;
		bool m_isBP;
		int m_counter_size;
		
		std::vector<FeatureEntry*> m_table;
		std::vector< std::vector<int> > stats_heatmap_weight;
		std::vector< std::vector<int> > stats_heatmap_allocation;
		std::vector< std::vector<int> > stats_allocation_buffer;
		std::vector< std::vector<int> > stats_weight_buffer;
};		



#endif 
