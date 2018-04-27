#include <string>
#include <assert.h>
#include <vector>
#include <map>
#include <ostream>
#include <iostream>

#include "FeatureTable.hh"

using namespace std;

void 
FeatureEntry::updateDatasetState()
{
	
	CostFunctionParameters params = simu_parameters.optTarget;
	
	double E_SRAM = (params.costDRAM[WRITE_ACCESS] + params.costSRAM[WRITE_ACCESS]) * \
		       		 (cpts[WRITE_ACCESS][RD_NOT_ACCURATE] + cpts[WRITE_ACCESS][RD_MEDIUM]) + \
		        (params.costDRAM[READ_ACCESS] + params.costSRAM[WRITE_ACCESS]) * \
		       		 (cpts[READ_ACCESS][RD_NOT_ACCURATE] + cpts[READ_ACCESS][RD_MEDIUM]) + \
		       params.costSRAM[READ_ACCESS] * cpts[READ_ACCESS][RD_SHORT] + \
		       params.costSRAM[WRITE_ACCESS] * cpts[WRITE_ACCESS][RD_SHORT];	

	double E_NVM = (params.costDRAM[WRITE_ACCESS] + params.costNVM[WRITE_ACCESS]) * \
	       		cpts[WRITE_ACCESS][RD_NOT_ACCURATE] + \
		        (params.costDRAM[READ_ACCESS] + params.costNVM[WRITE_ACCESS]) * \
		       		 cpts[READ_ACCESS][RD_NOT_ACCURATE] + \
		       params.costNVM[READ_ACCESS] *  ( cpts[READ_ACCESS][RD_MEDIUM] + cpts[READ_ACCESS][RD_SHORT]) + \
		       params.costNVM[WRITE_ACCESS] * ( cpts[WRITE_ACCESS][RD_MEDIUM] + cpts[WRITE_ACCESS][RD_SHORT]);

	if(E_SRAM > E_NVM)
		des = ALLOCATE_IN_SRAM;
	else
		des = ALLOCATE_IN_NVM;
		
		
	cpts = vector< vector<int> >(NUM_RW_TYPE, vector<int>(NUM_RD_TYPE, 0));
	cptWindow = 0;
}



FeatureTable::FeatureTable( int size, string name)
{
	m_size = size;
	m_name = name;
	
	m_table.resize(size);
	for(int i = 0 ; i < m_size; i++)
		m_table[i] = new FeatureEntry();

	if(simu_parameters.perceptron_drawFeatureMaps)
	{
		stats_heatmap = vector<vector<int> >(m_size, vector<int>());
		stats_frequencymap = vector<vector<int> >(m_size, vector<int>());
		stats_history_buffer = vector<vector<int> >(m_size, vector<int>());
	}
}

FeatureTable::~FeatureTable()
{
	for(auto entry: m_table)
		delete entry;		
}

FeatureEntry*
FeatureTable::lookup(int index)
{
	assert(index < m_size && "Error during Index Function");
	return m_table[index];
}	

void 
FeatureTable::recordAccess(int index, Access element, RD_TYPE rd)
{
	FeatureEntry* feature_entry = lookup(index);
	
	if(!feature_entry->isValid)
	{	
		//Init the feature entry
		feature_entry->cpts = vector< vector<int> >(NUM_RW_TYPE, vector<int>(NUM_RD_TYPE, 0));
		feature_entry->cptWindow = 0;
		feature_entry->isValid = true;
	}

	if( element.isWrite() ) 
		feature_entry->cpts[rd][WRITE_ACCESS]++;
	else
		feature_entry->cpts[rd][READ_ACCESS]++;
	
	feature_entry->cptWindow++;

	if(feature_entry->cptWindow > simu_parameters.perceptron_windowSize)
		feature_entry->updateDatasetState();
}

int
FeatureTable::getBypassPrediction(int index)
{
	int result = lookup(index)->bypass_counter;
	
	if(simu_parameters.perceptron_drawFeatureMaps)	
		stats_history_buffer[index].push_back(result);
			
	return result;
}

void
FeatureTable::decreaseConfidence(int index)
{
	FeatureEntry* feature_entry = lookup(index);
	feature_entry->bypass_counter--;
	if(feature_entry->bypass_counter < -simu_parameters.perceptron_counter_size)
		feature_entry->bypass_counter = -simu_parameters.perceptron_counter_size;	
}

void
FeatureTable::increaseConfidence(int index)
{
	FeatureEntry* feature_entry = lookup(index);
	feature_entry->bypass_counter++;
	if(feature_entry->bypass_counter > simu_parameters.perceptron_counter_size)
		feature_entry->bypass_counter = simu_parameters.perceptron_counter_size;	
}


void
FeatureTable::recordEvict(int index , bool hasBeenReused)
{

}

int
FeatureTable::getAllocationPrediction(int index)
{
	FeatureEntry* entry = lookup(index);
	
	if(!entry)
		return 0;
		
	if(entry->des == ALLOCATE_IN_NVM)
		return entry->allocation_counter;
	else if( entry->des == ALLOCATE_IN_SRAM)
		return (-1) * entry->allocation_counter;
	else
		return 0;
}


void 
FeatureTable::openNewTimeFrame()
{
	if(!simu_parameters.perceptron_drawFeatureMaps)
		return;
	int avg = 0;
	for(unsigned i = 0; i < stats_history_buffer.size() ; i++)
	{
		int sum = 0;
		if(stats_history_buffer[i].size() != 0)
		{
			for(unsigned j = 0 ; j < stats_history_buffer[i].size() ; j++)
				sum += stats_history_buffer[i][j];


			avg = sum / (int)stats_history_buffer[i].size();		
		}
		else 
			avg = 0;

		stats_heatmap[i].push_back(avg);
		stats_frequencymap[i].push_back(stats_history_buffer[i].size());
		stats_history_buffer[i].clear();
	}

//	stats_history_buffer = vector< vector<int> >(m_size , vector<int>());
}

void 
FeatureTable::finishSimu()
{

	if(!simu_parameters.perceptron_drawFeatureMaps || stats_heatmap.size() == 0)
		return;
		
	int height = m_size;
	int width = stats_heatmap[0].size();
	
	vector< vector<int> > red = vector< vector<int> >(width, vector<int>(height, 0));
	vector< vector<int> > blue  = red;
	vector< vector<int> > green = red;

	for(unsigned i = 0 ; i < stats_heatmap.size(); i++)
	{
		for(unsigned j = 0 ; j < stats_heatmap[i].size(); j++)
		{
			int value = stats_heatmap[i][j];
			if(value >= 0)
			{
				red[j][255-i] = 255;
				blue[j][255-i] = 255 - 255 * value / 32;			
				green[j][255-i] =  255 - 255 * value / 32; 
			}
			else
			{
				blue[j][255-i] = 255;
				red[j][255-i] = 255 - 255 * -value / 32;
				green[j][255-i] = 255 - 255 * -value / 32; 
			}
		}
	}

	writeBMPimage(string("heatmap_" + m_name + ".bmp") , width , height , red, blue, green );
	
	

	for(unsigned i = 0 ; i < stats_frequencymap.size(); i++)
	{
		for(unsigned j = 0 ; j < stats_frequencymap[i].size(); j++)
		{
			int value = stats_frequencymap[i][j];
			if(value > 5)
				value = 5;

				
			red[j][255-i] = 255;
			blue[j][255-i] = 255 - 255 * value / 5;			
			green[j][255-i] = 255 - 255 * value / 5; 
		}
	}

	writeBMPimage(string("frequencymap_" + m_name + ".bmp") , width , height , red, blue, green );
	
}
