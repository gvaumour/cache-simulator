#include <cmath>
#include <string>
#include <assert.h>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>

#include "FeatureTable.hh"
#include "common.hh"
using namespace std;

void 
FeatureEntry::updateDatasetState()
{
	
	CostFunctionParameters params = simu_parameters.optTarget;
	double E_SRAM = 0 , E_NVM = 0;
	
	if(simu_parameters.Cerebron_RDmodel)
	{
		
		E_SRAM = SRAM_seq[WRITE_ACCESS][false] * (params.costDRAM[WRITE_ACCESS] + params.costSRAM[WRITE_ACCESS]) +\
			 SRAM_seq[WRITE_ACCESS][true] * (params.costSRAM[WRITE_ACCESS]) +\
			 SRAM_seq[READ_ACCESS][false] * (params.costDRAM[READ_ACCESS] + params.costSRAM[READ_ACCESS]) +\
			 SRAM_seq[READ_ACCESS][true] * (params.costSRAM[READ_ACCESS]);

		E_NVM = NVM_seq[WRITE_ACCESS][false] * (params.costDRAM[WRITE_ACCESS] + params.costNVM[WRITE_ACCESS]) +\
			 NVM_seq[WRITE_ACCESS][true] * (params.costNVM[WRITE_ACCESS]) +\
			 NVM_seq[READ_ACCESS][false] * (params.costDRAM[READ_ACCESS] + params.costNVM[READ_ACCESS]) +\
			 NVM_seq[READ_ACCESS][true] * (params.costNVM[READ_ACCESS]);
	}
	else
	{
		E_SRAM = (params.costDRAM[WRITE_ACCESS] + params.costSRAM[WRITE_ACCESS]) * \
			       		 (cpts[WRITE_ACCESS][RD_NOT_ACCURATE] + cpts[WRITE_ACCESS][RD_MEDIUM]) + \
				(params.costDRAM[READ_ACCESS] + params.costSRAM[WRITE_ACCESS]) * \
			       		 (cpts[READ_ACCESS][RD_NOT_ACCURATE] + cpts[READ_ACCESS][RD_MEDIUM]) + \
			       params.costSRAM[READ_ACCESS] * cpts[READ_ACCESS][RD_SHORT] + \
			       params.costSRAM[WRITE_ACCESS] * cpts[WRITE_ACCESS][RD_SHORT];	

		E_NVM = (params.costDRAM[WRITE_ACCESS] + params.costNVM[WRITE_ACCESS]) * \
		       		cpts[WRITE_ACCESS][RD_NOT_ACCURATE] + \
				(params.costDRAM[READ_ACCESS] + params.costNVM[WRITE_ACCESS]) * \
			       		 cpts[READ_ACCESS][RD_NOT_ACCURATE] + \
			       params.costNVM[READ_ACCESS] *  ( cpts[READ_ACCESS][RD_MEDIUM] + cpts[READ_ACCESS][RD_SHORT]) + \
			       params.costNVM[WRITE_ACCESS] * ( cpts[WRITE_ACCESS][RD_MEDIUM] + cpts[WRITE_ACCESS][RD_SHORT]);	
	}

	if(E_SRAM > E_NVM)
		des = ALLOCATE_IN_SRAM;
	else
		des = ALLOCATE_IN_NVM;
		
		
	cpts = vector< vector<int> >(NUM_RW_TYPE, vector<int>(NUM_RD_TYPE, 0));
	cptWindow = 0;
}



FeatureTable::FeatureTable( int size, string name, bool isBP)
{
	m_size = size;
	m_name = name;
	m_isBP = isBP;
	m_counter_size = simu_parameters.perceptron_counter_size;
	
	m_table.resize(size);
	for(int i = 0 ; i < m_size; i++)
		m_table[i] = new FeatureEntry();

	if(simu_parameters.perceptron_drawFeatureMaps)
	{
		stats_heatmap_weight = vector<vector<int> >(m_size, vector<int>());
		stats_heatmap_allocation = vector<vector<int> >(m_size, vector<int>());
		stats_allocation_buffer = vector<vector<int> >(m_size, vector<int>());
		stats_weight_buffer = vector<vector<int> >(m_size, vector<int>());

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
FeatureTable::recordAccess(int index, bool isWrite, RD_TYPE rd)
{
	FeatureEntry* feature_entry = lookup(index);
	
	if(!feature_entry->isValid)
	{	
		//Init the feature entry
		feature_entry->cpts = vector< vector<int> >(NUM_RW_TYPE, vector<int>(NUM_RD_TYPE, 0));
		feature_entry->cptWindow = 0;
		feature_entry->isValid = true;
	}

	if( isWrite ) 
		feature_entry->cpts[WRITE_ACCESS][rd]++;
	else
		feature_entry->cpts[READ_ACCESS][rd]++;
	
	feature_entry->cptWindow++;

	if(feature_entry->cptWindow >= simu_parameters.perceptron_windowSize)
		feature_entry->updateDatasetState();
}

void
FeatureTable::recordAccess(int index, bool isWrite, bool isHitSRAM, bool isHitNVM)
{
	FeatureEntry* feature_entry = lookup(index);
	
	if(!feature_entry->isValid)
	{	
		//Init the feature entry
		feature_entry->cpts = vector< vector<int> >(NUM_RW_TYPE, vector<int>(NUM_RD_TYPE, 0));
		feature_entry->cptWindow = 0;
		feature_entry->isValid = true;
	}

	feature_entry->SRAM_seq[isWrite][isHitSRAM]++;
	feature_entry->NVM_seq[isWrite][isHitNVM]++;
	
	feature_entry->cptWindow++;

	if(feature_entry->cptWindow >= simu_parameters.perceptron_windowSize)
		feature_entry->updateDatasetState();
}


allocDecision
FeatureTable::getAllocDecision(int index, bool isWrite)
{
	FeatureEntry* entry = lookup(index);
	allocDecision result = entry->des;
	
	if(result == ALLOCATE_PREEMPTIVELY)
	{
		if(isWrite)
			result = ALLOCATE_IN_NVM;
		else 
			result = ALLOCATE_IN_SRAM;
	}

	if(simu_parameters.perceptron_drawFeatureMaps)
	{
		int sign = result == ALLOCATE_IN_NVM ? -1 : +1;
		stats_allocation_buffer[index].push_back( sign * 32);
	}


	return result;
}



void 
FeatureTable::decreaseAlloc(int index)
{
	FeatureEntry* entry = lookup(index);
	entry->allocation_counter--;
	
	if(entry->allocation_counter < -m_counter_size)
		entry->allocation_counter = -m_counter_size;
}

void 
FeatureTable::increaseAlloc(int index)
{
	FeatureEntry* entry = lookup(index);
	entry->allocation_counter++;
	
	if(entry->allocation_counter > m_counter_size)
		entry->allocation_counter = m_counter_size;	
}

int
FeatureTable::getAllocationPrediction(int index, bool isLegal = true)
{
	int result = lookup(index)->allocation_counter;
	
//	if(simu_parameters.perceptron_drawFeatureMaps && isLegal)
//		stats_allocation_buffer[index].push_back(result);
			
	return result;
}



int
FeatureTable::getConfidence(int index, bool isLegal)
{
	int result = lookup(index)->weight;
	
	if(simu_parameters.perceptron_drawFeatureMaps && isLegal)
		stats_weight_buffer[index].push_back(result);
			
	return result;
}


void
FeatureTable::decreaseConfidence(int index)
{
	FeatureEntry* feature_entry = lookup(index);
	feature_entry->weight--;
	if(feature_entry->weight < 0)
		feature_entry->weight = 0;	
}

void
FeatureTable::increaseConfidence(int index)
{
	FeatureEntry* feature_entry = lookup(index);
	feature_entry->weight++;
	if(feature_entry->weight > simu_parameters.perceptron_counter_size)
		feature_entry->weight = simu_parameters.perceptron_counter_size;	
}

void
FeatureTable::registerError(int index , bool isError)
{
	/*
	if(isError)
		stats_errorMap[index][stats_errorMap.size()-1]++;
	*/
}


void
FeatureTable::recordEvict(int index , bool hasBeenReused)
{

}

void 
FeatureTable::openNewTimeFrame()
{
	if(!simu_parameters.perceptron_drawFeatureMaps)
		return;
	int avg = 0;
	for(unsigned i = 0; i < stats_allocation_buffer.size() ; i++)
	{
		int sum = 0;
		if(stats_allocation_buffer[i].size() != 0)
		{
			for(unsigned j = 0 ; j < stats_allocation_buffer[i].size() ; j++)
				sum += stats_allocation_buffer[i][j];


			avg = sum / (int)stats_allocation_buffer[i].size();		
		}
		else 
			avg = stats_heatmap_allocation[i].empty() ? 0 : stats_heatmap_allocation[i].back() ;

		stats_heatmap_allocation[i].push_back(avg);

		stats_allocation_buffer[i].clear();
	}

	
	for(unsigned i = 0; i < stats_weight_buffer.size() ; i++)
	{
		int sum = 0;
		if(stats_weight_buffer[i].size() != 0)
		{
			for(unsigned j = 0 ; j < stats_weight_buffer[i].size() ; j++)
				sum += stats_weight_buffer[i][j];


			avg = sum / (int)stats_weight_buffer[i].size();		
		}
		else 
			avg = stats_heatmap_weight[i].empty() ? 16 : stats_heatmap_weight[i].back() ;

		stats_heatmap_weight[i].push_back(avg);

		stats_weight_buffer[i].clear();
	}


}

void 
FeatureTable::finishSimu()
{

	if(!simu_parameters.perceptron_drawFeatureMaps || stats_heatmap_allocation.size() == 0)
		return;
		
	int height = m_size;
	int width = stats_heatmap_allocation[0].size();


	if(width > IMG_WIDTH)
	{
		stats_heatmap_allocation = resize_image(stats_heatmap_allocation);
		stats_heatmap_weight = resize_image(stats_heatmap_weight);
		width = stats_heatmap_allocation[0].size();
	}	
	vector< vector<int> > red = vector< vector<int> >(width, vector<int>(height, 0));
	vector< vector<int> > blue  = red;
	vector< vector<int> > green = red;
	
	for(unsigned i = 0 ; i < stats_heatmap_allocation.size(); i++)
	{
		for(unsigned j = 0 ; j < stats_heatmap_allocation[i].size(); j++)
		{
			int value = stats_heatmap_allocation[i][j];
			
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

	writeBMPimage(string("allocation_" + m_name + ".bmp") , width , height , red, blue, green );
	
	for(unsigned i = 0 ; i < stats_heatmap_weight.size(); i++)
	{
		for(unsigned j = 0 ; j < stats_heatmap_weight[i].size(); j++)
		{
			int value = stats_heatmap_weight[i][j] - simu_parameters.perceptron_counter_size/2;
			
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

	writeBMPimage(string("weight_" + m_name + ".bmp") , width , height , red, blue, green );
}








