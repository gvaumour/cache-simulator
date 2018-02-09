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
#include <stdint.h>
#include <string>

#ifdef TEST
	#define DPRINTF(...) printf(__VA_ARGS__);
#else
	#define DPRINTF(...) 
#endif

#define CONFIG_FILE "config.ini"
#define OUTPUT_FILE "results.out"
#define LOG_FILE "log.out"

#define PREDICTOR_TIME_FRAME 1E5

#define ONE_MB 1048576
#define TWO_MB 2*ONE_MB
#define FOUR_MB 2*TWO_MB


#define RAP_DEAD_COUNTER_SATURATION  3
#define RAP_LEARNING_THRESHOLD  20
#define RAP_WINDOW_SIZE  20
#define RAP_INACURACY_TH  0.7

/* Used by the prefetcher */ 
#define PAGE_SIZE 4096
#define PAGE_MASK ~(PAGE_SIZE-1)


#define BLOCK_SIZE 64

/**
	Hold utilitary functions 
*/

struct SimuParameters
{
	bool enableBP;
	bool enableMigration;
	
	bool printDebug;
	
	int deadSaturationCouter;
	int window_size;
	int learningTH;
	
	int sram_assoc;
	int nvm_assoc;
	int nb_sets;
	int nb_bits;

	int rap_assoc;
	int rap_sets;
	
	bool flagTest;

	bool enablePrefetch;
	int prefetchDegree;
	int prefetchStreams;

	float rap_innacuracy_th;
	
	std::vector<std::string> memory_traces;
	int nbCores;
	std::string policy;

	int saturation_threshold;
	int cost_threshold;

	int sizeMTtags;

};

std::vector<std::string> split(std::string s, char delimiter);
uint64_t bitRemove(uint64_t address , unsigned int small, unsigned int big);
uint64_t hexToInt(std::string adresse_hex);
uint64_t hexToInt1(const char* adresse_hex);

bool readInputArgs(int argc , char* argv[] , int& sizeCache , int& assoc , int& blocksize, std::string& filename, std::string& policy);
bool isPow2(int x);
std::string convert_hex(int n);
const char * StripPath(const char * path);
bool isPolicyDynamic(std::string policy);
void init_default_parameters();


extern uint64_t cpt_time;
extern int start_debug;

extern const char* memCmd_str[];
extern const char* allocDecision_str[];
extern const char* directory_state_str[];

extern SimuParameters simu_parameters;

#endif 
