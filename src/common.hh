/** 
Copyright (C) 2016 Gregory Vaumourin

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/

#ifndef COMMON_HPP
#define COMMON_HPP

#include <vector>
#include <set>
#include <stdint.h>
#include <string>

#include "Cache.hh"

#define DPRINTF(FLAG, ...) if(simu_parameters.enable_debugflags.count(#FLAG) != 0) {printf(__VA_ARGS__);} 
#define BOOL_STR(a) if(a){"TRUE";}else{"FALSE";};

#define CONFIG_FILE "config.ini"
#define OUTPUT_FILE "results.out"

#define PREDICTOR_TIME_FRAME 1E5
#define NB_ACCESS 500E6

#define ONE_MB 1048576
#define TWO_MB 2*ONE_MB
#define FOUR_MB 2*TWO_MB

/* Used by the prefetcher */ 
#define PAGE_SIZE 4096
#define PAGE_MASK ~(PAGE_SIZE-1)

#define BLOCK_SIZE 64

#define IMG_WIDTH 2000

/**
	Hold utilitary functions 
*/


struct SimuParameters
{
	bool printDebug;
	std::set<std::string> enable_debugflags;

	int sram_assoc;	
	int nbCores;
	int nvm_assoc;
	int nb_sets;
	int nb_sampled_sets;
	
	std::vector<std::string> memory_traces;

	std::string policy;

	bool simulate_conflicts;
	bool traceLLC;

	bool enablePrefetch;
	int prefetchDegree;
	int prefetchStreams;

	/*** DBAMB Parameters */ 	
	bool enableReuseErrorComputation;
	bool enablePCHistoryTracking;

	float rap_innacuracy_th;
	int deadSaturationCouter;
	int window_size;
	int learningTH;

	bool enableBP;
	bool enableMigration;
	
	int rap_assoc;
	int rap_sets;
	
	bool flagTest;

	int saturation_threshold;
	int cost_threshold;

	int size_MT_SRAMtags;
	int size_MT_NVMtags;
	unsigned MT_counter_th;
	uint64_t MT_timeframe;
	int nb_MTcouters_sampling;
	bool enableFullSRAMtraffic;
			
	float ratio_RWcost;	
	
	CostFunctionParameters optTarget;
	std::string DBAMB_signature;
	int DBAMB_begin_bit;
	int DBAMB_end_bit;
	bool enableDatasetSpilling;
	bool readDatasetFile;
	bool writeDatasetFile;
	std::string datasetFile;
	/******************************/ 
	
	std::vector<std::string> perceptron_BP_features;
	std::vector<std::string> perceptron_Allocation_features;
	int perceptron_counter_size;
	int perceptron_windowSize;
	int perceptron_bypass_threshold;
	int perceptron_bypass_learning;
	int perceptron_allocation_threshold;
	int perceptron_allocation_learning;
	int perceptron_table_size;
	bool perceptron_drawFeatureMaps;
	bool perceptron_compute_variance;
	bool perceptron_enableBypass;
	
	/***********/
	std::vector<std::string> PHC_features;
	int PHC_cost_threshold;
	int PHC_cost_mediumRead, PHC_cost_mediumWrite;
	int PHC_cost_shortWrite, PHC_cost_shortRead;
	int mediumrd_def;
	
	/****/
	std::string Cerebron_activation_function;
	bool Cerebron_independantLearning; 
	bool Cerebron_lowConfidence; 
	bool Cerebron_resetEnergyValues;
	bool Cerebron_separateLearning;
	bool Cerebron_fastlearning;
	bool Cerebron_RDmodel;
	int Cerebron_decrement_value;
	
	/****************/ 
	std::vector<std::string> SimplePerceptron_write_features;
	std::vector<std::string> SimplePerceptron_reuse_features;
	int SimplePerceptron_write_threshold;
	
};

std::vector<std::string> split(std::string s, char delimiter);
uint64_t bitRemove(uint64_t address , unsigned int small, unsigned int big);
uint64_t bitSelect(uint64_t address , unsigned small , unsigned big);

uint64_t hexToInt(std::string adresse_hex);
uint64_t hexToInt1(const char* adresse_hex);

bool readInputArgs(int argc , char* argv[] , int& sizeCache , int& assoc , int& blocksize, std::string& filename, std::string& policy);
bool isPow2(int x);
std::string convert_hex(int n);
const char * StripPath(const char * path);
void init_default_parameters();
std::string buildHash(uint64_t a, uint64_t p);
std::vector<std::string> splitAddr_Bytes(uint64_t a);


std::vector< std::vector<int> > resize_image(std::vector< std::vector<int> >& img);

void writeBMPimage(std::string image_name , int width , int height , std::vector< std::vector<int> > red,\
				 std::vector< std::vector<int> > blue, std::vector< std::vector<int> > green );
std::string splitFilename(std::string str);


extern uint64_t cpt_time;
extern int start_debug;
extern int miss_counter;

extern const char* memCmd_str[];
extern const char* str_RW_status[];
extern const char* str_RD_status[];
extern const char* str_allocDecision[];
extern const char* directory_state_str[];
extern std::set<std::string> simulation_debugflags;

extern SimuParameters simu_parameters;

#endif
