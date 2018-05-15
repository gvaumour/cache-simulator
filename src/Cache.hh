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


#ifndef CACHE_HPP
#define CACHE_HPP

#include <map>
#include <vector>
#include <ostream>

#define READ_ACCESS 0
#define WRITE_ACCESS 1


enum MemCmd{
	INST_READ,
	INST_PREFETCH,
	DATA_READ,
	DATA_WRITE,
	DATA_PREFETCH,
	CLEAN_WRITEBACK,
	DIRTY_WRITEBACK,
	SILENT_WRITEBACK,
	INSERT,
	EVICTION,
	ACE,
	NUM_MEM_CMDS
};

enum allocDecision
{	
	ALLOCATE_IN_SRAM = 0,
	ALLOCATE_IN_NVM,
	BYPASS_CACHE,
	ALLOCATE_PREEMPTIVELY,
	NUM_ALLOC_DECISION
};

enum RD_TYPE
{
	RD_SHORT = 0,
	RD_MEDIUM,
	RD_NOT_ACCURATE,
	NUM_RD_TYPE
};

enum RW_TYPE
{
	DEAD = 0,
	WO,
	RO,
	RW,
	RW_NOT_ACCURATE,
	NUM_RW_TYPE
};


enum CoherenceState
{
	COHERENCE_MODIFIED,
	COHERENCE_EXCLUSIVE,
	COHERENCE_SHARED,
	COHERENCE_INVALID,
	NUM_CoherenceState
};


enum DirectoryState
{
	SHARED_L1,
	MODIFIED_L1,
	EXCLUSIVE_L1,

	SHARED_L1_NOT_LLC,
	MODIFIED_L1_NOT_LLC,
	EXCLUSIVE_L1_NOT_LLC,

	CLEAN_LLC,
	DIRTY_LLC,
	NOT_PRESENT,
	NUM_DirectoryState
};

struct HistoEntry
{
	RW_TYPE state_rw;
	RD_TYPE state_rd;
	int nbKeepState;
};


class Access{
	
	public : 
		Access() : m_address(0), m_size(0), m_pc(0), m_type(DATA_READ), m_idthread(0),isSRAMerror(false) {};
		Access(uint64_t address, int size, uint64_t pc , MemCmd type, int id_thread) : m_address(address), m_size(size), \
				m_pc(pc), m_hints(0), m_type(type) , m_idthread(id_thread), isSRAMerror(false), enableMigration(true) {};

		bool isWrite() { return m_type == MemCmd::DATA_WRITE || m_type == MemCmd::DIRTY_WRITEBACK;}
		bool isInstFetch() { return m_type == MemCmd::INST_READ || m_type == MemCmd::INST_PREFETCH;}
		bool isPrefetch() { return m_type == MemCmd::DATA_PREFETCH || m_type == MemCmd::INST_PREFETCH;}
		bool isDemandAccess() { return m_type == MemCmd::DATA_WRITE ||  m_type == MemCmd::DATA_READ ||  m_type == MemCmd::INST_READ;}
		
		void print(std::ostream& out) const;

		
		uint64_t m_address;
		int m_size;
		uint64_t m_pc;
		int m_hints;
		MemCmd m_type;		
		unsigned m_idthread;
		int m_compilerHints;
		bool isSRAMerror;
		bool enableMigration;
};






class CacheEntry{
	public :
		CacheEntry() { 
			isNVM = false;
			isLearning = false;
			initEntry();
		};


		void initEntry() {
			isValid = false; 
			isDirty = false;
			lastWrite = false;
			
			isPrefetch = false;
			address = 0;
			policyInfo = 0; 
			saturation_counter = 0;
			m_pc = 0;
			nbRead = 0;
			nbWrite = 0;
			m_compilerHints = 0;
			justMigrate = false;
			value = 0;
			cost_value = 0;
			signature = 0;
			last_time_access = 0;
			coherence_state = COHERENCE_INVALID;
		}
		
		void copyCL(CacheEntry* a)
		{
			address = a->address;
			isDirty = a->isDirty;
			nbWrite = a->nbWrite;
			nbRead = a->nbRead;
			lastWrite = a->lastWrite;
			m_compilerHints = a->m_compilerHints;
			isLearning = a->isLearning;
			m_pc = a->m_pc;
			coherence_state = a->coherence_state; 
			isPrefetch = a->isPrefetch;
			pc_history = a->pc_history;
		}
		bool isValid;
		bool isDirty;
		uint64_t address;
		uint64_t m_pc;
		uint64_t signature;
		uint64_t value;
		uint64_t last_time_access;
		
		int policyInfo;
		int m_compilerHints;
		bool isNVM;

		std::vector<int> perceptron_BPpred;
		std::vector<int> perceptron_Allocpred;
		
		std::vector<int> perceptron_BPHash;
		std::vector<int> perceptron_AllocHash;

		std::vector<bool> predictedReused;
		
		int nbWrite;
		int nbRead;
		bool lastWrite;
		
		//field used only by the SaturationCounter Predictor
		int saturation_counter; 
		int cost_value; 
		
		//field used only by the RAP predictor
		bool isLearning;
		bool isPrefetch;
		bool justMigrate;
		std::vector<uint64_t> pc_history;

		CoherenceState coherence_state;
};


typedef std::vector<std::vector<CacheEntry*> > DataArray;


class ConfigCache{
	public : 
		ConfigCache(): m_size(0), m_assoc(0), m_blocksize(0), m_policy(""), m_nbNVMways(0), m_printStats(false) {};
		ConfigCache(int size , int assoc , int blocksize, std::string policy, int nbNVMways) : \
		m_size(size), m_assoc(assoc), m_blocksize(blocksize), m_policy(policy), m_nbNVMways(nbNVMways), m_printStats(false) {};
		
		ConfigCache(const ConfigCache& a): m_size(a.m_size), m_assoc(a.m_assoc), m_blocksize(a.m_blocksize),\
					 m_policy(a.m_policy), m_nbNVMways(a.m_nbNVMways), m_printStats(a.m_printStats) {};		

		int m_size;
		int m_assoc;
		int m_blocksize;
		std::string m_policy;	
		int m_nbNVMways;
		bool m_printStats;
};

class CostFunctionParameters
{
	public:
		CostFunctionParameters(){};
		std::vector<double> costNVM; 
		std::vector<double> costSRAM;
		std::vector<double> costDRAM;
};

class EnergyParameters: public CostFunctionParameters
{
	public: 
		EnergyParameters() {

			costSRAM = std::vector<double>(2 ,0);
			costSRAM[READ_ACCESS] = 271.678E-12;
			costSRAM[WRITE_ACCESS] = 257.436E-12;

			costNVM = std::vector<double>(2 ,0);
			costNVM[READ_ACCESS] = 236.828E-12;
			costNVM[WRITE_ACCESS] = 652.278E-12;

			costDRAM = std::vector<double>(2 ,0);
			costDRAM[READ_ACCESS] = 4.08893E-9;
			costDRAM[WRITE_ACCESS] = 4.10241E-9;

		};
};

class PerfParameters: public CostFunctionParameters
{
	public: 
		PerfParameters() {

			costSRAM = std::vector<double>(2 ,0);
			costSRAM[READ_ACCESS] = 10;
			costSRAM[WRITE_ACCESS] = 10;

			costNVM = std::vector<double>(2 ,0);
			costNVM[READ_ACCESS] = 10;
			costNVM[WRITE_ACCESS] = 37;

			costDRAM = std::vector<double>(2 ,0);
			costDRAM[READ_ACCESS] = 100;
			costDRAM[WRITE_ACCESS] = 100;
		};
};



#endif

