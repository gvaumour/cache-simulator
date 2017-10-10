#include <math.h>
#include <iostream>
#include <string>
#include <vector>

#include "Hierarchy.hh"
#include "HybridCache.hh"
#include "common.hh"

using namespace std;


Hierarchy::Hierarchy()
{
	DPRINTF("Hierarchy:: null constructor\n");

	Hierarchy("RAP" , 1);
}

Hierarchy::Hierarchy(const Hierarchy& a)
{
	DPRINTF("Hierarchy:: copy constructor\n");

}


Hierarchy::Hierarchy(string policy, int nbCores)
{
	ConfigCache L1Dataconfig (32768, 2 , BLOCK_SIZE , "LRU", 0);
	vector<ConfigCache> firstLevel;
	firstLevel.push_back(L1Dataconfig);

	ConfigCache L1Instconfig = L1Dataconfig;
	firstLevel.push_back(L1Instconfig);

	ConfigCache L2config ( TWO_MB , simu_parameters.sram_assoc + simu_parameters.nvm_assoc, \
				BLOCK_SIZE , policy , simu_parameters.nvm_assoc);
	L2config.m_printStats = true;
	vector<ConfigCache> secondLevelConfig;
	secondLevelConfig.push_back(L2config);
	
		
	m_nbLevel = 2;
	m_nbCores = nbCores;
//	m_private_caches.resize(m_nbCores);
	
	m_directory = new Directory();
	
	stats_beginTimeFrame = 0;
	
	for(unsigned i = 0 ; i < m_nbCores ; i++){
		m_private_caches.push_back(new Level(i , firstLevel, this) );
	}

	m_LLC = new Level(-1, secondLevelConfig , this);
	
	m_start_index = log2(BLOCK_SIZE)-1;
	
	stats_cleanWB_MainMem = 0;
	stats_dirtyWB_MainMem = 0;
}


Hierarchy::~Hierarchy()
{
	for(auto p : m_private_caches)
		delete p;
	delete m_LLC;
}


void
Hierarchy::startWarmup()
{
	for(unsigned j = 0 ; j < m_private_caches.size() ; j++)
		m_private_caches[j]->startWarmup();

	m_LLC->startWarmup();
}

void
Hierarchy::stopWarmup()
{
	for(unsigned j = 0 ; j < m_private_caches.size() ; j++)
		m_private_caches[j]->stopWarmup();
	m_LLC->stopWarmup();
}


void
Hierarchy::printResults(ostream& out)
{
	out << "***************" << endl;
	for(unsigned j = 0 ; j < m_private_caches.size() ; j++)
	{
		out << "Core n°" << j << endl;		
		m_private_caches[j]->printResults(out);
	}
	out << "***************" << endl;
	out << "Last Level Cache : " << endl;
	m_LLC->printResults(out);
	out << "***************" << endl;
	out << "Main Mem" << endl;
	out << "\tClean WB\t" << stats_cleanWB_MainMem << endl;
	out << "\tDirty WB\t" << stats_dirtyWB_MainMem << endl;
	
}

void
Hierarchy::printConfig(ostream& out)
{
	out << "ConfigCache : " << m_configFile << endl;
	out << "NbLevel : " << m_nbLevel << endl;
	out << "NbCore : " << m_nbCores << endl;
	
	for(unsigned j = 0 ; j < m_private_caches.size() ; j++)
	{
		out << "Core n°" << j << endl;		
		m_private_caches[j]->printConfig(out);
	}
	out << "***************" << endl;
	out << "Last Level Cache : " << endl;
	m_LLC->printConfig(out);
}

void
Hierarchy::print(std::ostream& out) 
{
	printResults(out);
}

void
Hierarchy::signalWB(uint64_t addr, bool isDirty , bool isKept, int idcore)
{

	if(idcore == -1)
	{	
		// It is a WB from the LLC to the Main Mem
		if(isDirty)
			stats_dirtyWB_MainMem++;		
		else
			stats_cleanWB_MainMem++;
			
		m_directory->removeEntry(addr);
	}			
	else{
		//Remove the idcore from the tracker list and update the state
		if(!isKept)
			m_directory->removeTracker(addr , idcore);
		m_LLC->handleWB(addr , isDirty);
		m_directory->updateEntry(addr);
	}
}

void 
Hierarchy::finishSimu()
{
	for(unsigned j = 0 ; j < m_private_caches.size() ; j++)
	{
		m_private_caches[j]->finishSimu();
	}
	m_LLC->finishSimu();
}


void
Hierarchy::handleAccess(Access element)
{
	DPRINTF("HIERARCHY::New Access : Data %#lx Req %s, Id_Thread %d\n", element.m_address , memCmd_str[element.m_type], element.m_idthread);
	
	uint64_t addr = element.m_address;
	uint64_t block_addr = bitRemove(addr , 0 , m_start_index+1);
	int id_thread = (int) element.m_idthread;
	int id_core = convertThreadIDtoCore(id_thread);
	
	assert(id_core < (int)m_nbCores);	
	
	DirectoryEntry* entry = m_directory->getEntry(block_addr);
	if(entry == NULL){
		entry = m_directory->addEntry(block_addr, element.isInstFetch());
	}
	assert(entry != NULL);
		
	DirectoryState dir_state = m_directory->getEntry(block_addr)->coherence_state;
	
	entry->print();
	
	if(dir_state == NOT_PRESENT)
	{
		m_directory->setTrackerToEntry(block_addr, id_core);			

		m_private_caches[id_core]->handleAccess(element);		
		m_LLC->handleAccess(element);
		m_directory->updateEntry(block_addr);

		m_directory->setCoherenceState(block_addr, EXCLUSIVE_L1);
	}
	else if(dir_state == CLEAN_LLC || dir_state == DIRTY_LLC)
	{
			m_directory->setCoherenceState(block_addr, EXCLUSIVE_L1);
			m_directory->setTrackerToEntry(block_addr, id_core);			
	
			m_private_caches[id_core]->handleAccess(element);
			m_LLC->handleAccess(element);
			m_directory->updateEntry(block_addr);
	}
	else if( dir_state == MODIFIED_L1 || dir_state == EXCLUSIVE_L1)
	{
		set<int> nodes = m_directory->getTrackers(block_addr);
		assert(nodes.size() == 1);
		int node = *(nodes.begin());

		if(node != id_core)
		{
			m_private_caches[node]->sendInvalidation(block_addr, element.isInstFetch() );
			//If it is a write, deallocate it from one core to allocate it to the other
			if(element.isWrite()){
				m_private_caches[node]->deallocate(block_addr);
				m_directory->setTrackerToEntry(block_addr, id_core);
				m_directory->setCoherenceState(block_addr, EXCLUSIVE_L1);

			}
			else
			{	//If it is a read, add id_core to the tracker list 
				m_directory->addTrackerToEntry(block_addr, id_core);
				m_directory->setCoherenceState(block_addr, SHARED_L1);

			}
			m_LLC->handleAccess(element);
			m_directory->updateEntry(block_addr);
			m_private_caches[id_core]->handleAccess(element);
		
		}
		else{
			// Transistion from Exclusive to Modified 
			if(element.isWrite() && dir_state == EXCLUSIVE_L1)
				m_directory->setCoherenceState(block_addr, MODIFIED_L1);

			m_private_caches[id_core]->handleAccess(element);
		
		}
	}
	else if(dir_state == SHARED_L1)
	{
		if(element.isWrite())
		{
			//We invalidate all the sharers
			set<int> nodes = m_directory->getTrackers(block_addr);
			for(auto node : nodes)
				m_private_caches[node]->deallocate(block_addr);

			m_directory->resetTrackersToEntry(block_addr);
			m_directory->setTrackerToEntry(block_addr, id_core);
			m_directory->setCoherenceState(block_addr, EXCLUSIVE_L1);
			
			//We write the new version to the LLC 
			m_LLC->handleAccess(element);
			m_directory->updateEntry(block_addr);
			m_private_caches[id_core]->handleAccess(element);
		}
		else
		{
			// We add id_core to tracker list
			m_directory->addTrackerToEntry(block_addr, id_core);				
			m_private_caches[id_core]->handleAccess(element);
		}
	}


	if( (cpt_time - stats_beginTimeFrame) > PREDICTOR_TIME_FRAME)
		openNewTimeFrame();
	
	DPRINTF("HIERARCHY::End of handleAccess\n");
}


void
Hierarchy::openNewTimeFrame()
{
		for(unsigned i = 0 ; i < m_private_caches.size() ; i++)
		{
			m_private_caches[i]->openNewTimeFrame();
		}
		
		m_LLC->openNewTimeFrame();
		
		stats_beginTimeFrame = cpt_time;
}

void
Hierarchy::L1sdeallocate(uint64_t addr)
{
	
	//Invalidation of a cache line in LLC, need to invalidate all trackers in L1s
	DirectoryEntry* entry = m_directory->getEntry(addr);
	if( entry->isInL1() )
	{
		DPRINTF("Hierarchy::Deallocation of the block[%#lx] from LLC, invalidation of L1s\n" , addr);
		entry->print();
		
		set<int> nodes = m_directory->getTrackers(addr);

		for(auto node : nodes)
			m_private_caches[node]->sendInvalidation(addr, entry->isInst);

		for(auto node : nodes)
			m_private_caches[node]->deallocate(addr);
		
		m_directory->resetTrackersToEntry(addr);
		m_directory->setCoherenceState(addr , CLEAN_LLC);
	}		
}

int
Hierarchy::convertThreadIDtoCore(int id_thread)
{
	return id_thread % m_nbCores;
}



vector<ConfigCache>
Hierarchy::readConfigFile(string configFile)
{
	vector<ConfigCache> result;
	// Produce the ConfigCache to send to create the levels
	return result;
}

