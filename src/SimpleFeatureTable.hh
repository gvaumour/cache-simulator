#ifndef __FEATURE__TABLE__HH__
#define __FEATURE__TABLE__HH__

#include <vector>
#include <string>

#include "Cache.hh"
#include "common.hh"

class SimpleFeatureEntry
{
	public: 
		SimpleFeatureEntry() { initEntry(); isValid = false; };
		
		void initEntry() {
			counter = 0;
		 };
		
		bool isValid;
		int counter;
};


class SimpleFeatureTable
{

	public: 
		SimpleFeatureTable( int size, std::string name , bool isBP);
		~SimpleFeatureTable();

		SimpleFeatureEntry* lookup(int index);

		void openNewTimeFrame();
		void finishSimu();
		
		void recordEvict(int index , bool hasBeenReused);
		void recordAccess(int index, bool isWrite, RD_TYPE rd);
		
		void decreaseCounter(int hash);
		void increaseCounter(int hash);
		int getCounter(int hash);

		SimpleFeatureEntry* getEntry(int hash){ return m_table[hash];};
		
	private:
		int m_size;
		std::string m_name;
		int m_counter_size;
		
		std::vector<SimpleFeatureEntry*> m_table;
		std::vector< std::vector<int> > stats_heatmap_counter;
		std::vector< std::vector<int> > stats_counter_buffer;
};		



#endif 
