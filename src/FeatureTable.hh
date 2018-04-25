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
			dead_counter = 0;
			cptBypassLearning = 0;
			allocation_counter = 0;
			bypass_counter = 0;
			des = ALLOCATE_PREEMPTIVELY;
		 };
		
		void updateDatasetState();
		
		std::vector< std::vector<int> > cpts;

		int cptBypassLearning;		
		int cptWindow;
		
		bool isValid;
		int dead_counter;
		
		int allocation_counter;
		int bypass_counter;
		allocDecision des;
};


class FeatureTable
{

	public: 
		FeatureTable( int size, std::string name);
		~FeatureTable();

		FeatureEntry* lookup(int index);
		int getAllocationPrediction(int index);
		int getBypassPrediction(int index);

		void recordEvict(int index , bool hasBeenReused);		
		void recordAccess(int index, Access element, RD_TYPE rd);

		void decreaseConfidence(int index);
		void increaseConfidence(int index);
		
	private:
		int m_size;
		std::string m_name;
		std::vector<FeatureEntry*> m_table;
};		



#endif 
