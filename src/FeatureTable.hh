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
		 	cpts =  std::vector< std::vector<int> >(NUM_RW_TYPE, std::vector<int>(NUM_RD_TYPE, 0));
		 					
			isValid = true;
			cptWindow = 0;			
//			allocation_counter = 0;
//			bypass_counter = 0;
			weight = 0;
			des = ALLOCATE_PREEMPTIVELY;
		 };
		
		void updateDatasetState();
		
		std::vector< std::vector<int> > cpts;

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

		int getConfidence(int index);

		void openNewTimeFrame();
		void finishSimu();
		void recordEvict(int index , bool hasBeenReused);		
		void recordAccess(int index, Access element, RD_TYPE rd);

//		void decreaseBPConfidence(int index);
//		void increaseBPConfidence(int index);

		void decreaseConfidence(int index);
		void increaseConfidence(int index);
		
		void registerError(int index , bool isError);

		void decreaseAlloc(int index);
		void increaseAlloc(int index);
		int getAllocationPrediction(int index);
		
//		std::vector< std::vector<int> > resize_image(std::vector< std::vector<int> >& img);

		
	private:
		int m_size;
		std::string m_name;
		bool m_isBP;
		int m_counter_size;
		
		std::vector<FeatureEntry*> m_table;
		std::vector< std::vector<int> > stats_heatmap;
//		std::vector< std::vector<int> > stats_frequencymap;
		std::vector< std::vector<int> > stats_history_buffer;
		std::vector< std::vector<int> > stats_errorMap;
};		



#endif 
