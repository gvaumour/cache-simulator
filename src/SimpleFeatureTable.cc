#include <cmath>
#include <string>
#include <assert.h>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>

#include "SimpleFeatureTable.hh"
#include "common.hh"

using namespace std;


SimpleFeatureTable::SimpleFeatureTable( int size, string name, bool isBP)
{
	m_size = size;
	m_name = name;
	m_counter_size = simu_parameters.perceptron_counter_size;
	
	m_table.resize(size);
	for(int i = 0 ; i < m_size; i++)
		m_table[i] = new SimpleFeatureEntry();

	if(simu_parameters.perceptron_drawFeatureMaps)
	{
		stats_heatmap_counter = vector<vector<int> >(m_size, vector<int>());
		stats_counter_buffer = vector<vector<int> >(m_size, vector<int>());
	}
}

SimpleFeatureTable::~SimpleFeatureTable()
{
	for(auto entry: m_table)
		delete entry;		
}

SimpleFeatureEntry*
SimpleFeatureTable::lookup(int index)
{
	assert(index < m_size && "Error during Index Function");
	return m_table[index];
}	

void 
SimpleFeatureTable::recordAccess(int index, bool isWrite, RD_TYPE rd)
{
}



void
SimpleFeatureTable::decreaseCounter(int hash)
{
	SimpleFeatureEntry* entry = lookup(hash);
	entry->counter--;
	if(entry->counter < -m_counter_size)
		entry->counter = -m_counter_size;	
}	

void
SimpleFeatureTable::increaseCounter(int hash)
{
	SimpleFeatureEntry* entry = lookup(hash);
	entry->counter++;
	if(entry->counter > m_counter_size)
		entry->counter = m_counter_size;
}
		
int
SimpleFeatureTable::getCounter(int hash)
{
	SimpleFeatureEntry* entry = lookup(hash);
	if(simu_parameters.perceptron_drawFeatureMaps)
		stats_counter_buffer[hash].push_back( entry->counter );

	return  entry->counter;
}


void
SimpleFeatureTable::recordEvict(int index , bool hasBeenReused)
{

}

void 
SimpleFeatureTable::openNewTimeFrame()
{
	if(!simu_parameters.perceptron_drawFeatureMaps)
		return;
		
	int avg = 0;
	for(unsigned i = 0; i < stats_counter_buffer.size() ; i++)
	{
		int sum = 0;
		if(stats_counter_buffer[i].size() != 0)
		{
			for(unsigned j = 0 ; j < stats_counter_buffer[i].size() ; j++)
				sum += stats_counter_buffer[i][j];


			avg = sum / (int)stats_counter_buffer[i].size();		
		}
		else 
			avg = stats_heatmap_counter[i].empty() ? 0 : stats_heatmap_counter[i].back() ;

		stats_heatmap_counter[i].push_back(avg);
		stats_counter_buffer[i].clear();
	}

}

void 
SimpleFeatureTable::finishSimu()
{

	if(!simu_parameters.perceptron_drawFeatureMaps || stats_heatmap_counter.size() == 0)
		return;
		
	int height = m_size;
	int width = stats_heatmap_counter[0].size();


	if(width > IMG_WIDTH)
	{
		stats_heatmap_counter = resize_image(stats_heatmap_counter);
		width = stats_heatmap_counter[0].size();
	}	
	vector< vector<int> > red = vector< vector<int> >(width, vector<int>(height, 0));
	vector< vector<int> > blue  = red;
	vector< vector<int> > green = red;
	
	for(unsigned i = 0 ; i < stats_heatmap_counter.size(); i++)
	{
		for(unsigned j = 0 ; j < stats_heatmap_counter[i].size(); j++)
		{
			int value = stats_heatmap_counter[i][j];
			
			if(value >= 0)
			{
				red[j][i] = 255;
				blue[j][i] = 255 - 255 * value / 32;			
				green[j][i] =  255 - 255 * value / 32; 
			}
			else
			{
				blue[j][i] = 255;
				red[j][i] = 255 - 255 * -value / 32;
				green[j][i] = 255 - 255 * -value / 32; 
			}
		}
	}

	writeBMPimage(string("counter_" + m_name + ".bmp") , width , height , red, blue, green );
}








