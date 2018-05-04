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

#define CONFIG_FILE "config.ini"
#define OUTPUT_FILE "results.out"

#define PREDICTOR_TIME_FRAME 1E5

#define ONE_MB 1048576
#define TWO_MB 2*ONE_MB
#define FOUR_MB 2*TWO_MB

/* Used by the prefetcher */ 
#define PAGE_SIZE 4096
#define PAGE_MASK ~(PAGE_SIZE-1)

#define BLOCK_SIZE 64

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

	int sizeMTtags;
	
	float ratio_RWcost;	
	
	CostFunctionParameters optTarget;
	std::string DBAMB_signature;
	int DBAMB_begin_bit;
	int DBAMB_end_bit;
	
	bool readDatasetFile;
	bool writeDatasetFile;
	std::string datasetFile;
	/******************************/ 
	
	std::vector<std::string> perceptron_features;
	int perceptron_counter_size;
	int perceptron_windowSize;
	int perceptron_threshold_bypass;
	int perceptron_threshold_learning;
	int perceptron_table_size;
	bool perceptron_drawFeatureMaps;
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
void writeBMPimage(std::string image_name , int width , int height , std::vector< std::vector<int> > red,\
				 std::vector< std::vector<int> > blue, std::vector< std::vector<int> > green );


extern uint64_t cpt_time;
extern int start_debug;
extern int miss_counter;

extern const char* memCmd_str[];
extern const char* str_RW_status[];
extern const char* str_RD_status[];
extern const char* allocDecision_str[];
extern const char* directory_state_str[];
extern std::set<std::string> simulation_debugflags;

extern SimuParameters simu_parameters;

#endif
