#include <iostream>
#include <assert.h>
#include <map>
#include <vector>
#include <set>

#include "Directory.hh"
#include "common.hh"

using namespace std;


void
DirectoryEntry::print()
{
	cout << "DIRECTORYENTRY::Addr=0x" << std::hex << 	block_addr << \
	dec << ", State=" << directory_state_str[coherence_state];
	cout << ", Tracker=";

	for(auto p : nodeTrackers)		
		cout << p << ",";			
	cout << std::endl;

}

Directory::Directory()
{
	m_nb_set = DIRECTORY_NB_SETS;
	m_assoc = DIRECTORY_ASSOC;

	DPRINTF("DIRECTORY::Constructor , assoc=%d , m_nb_set=%d\n", m_assoc , m_nb_set);

	m_table.resize(m_nb_set);
	for(int i = 0  ; i < m_nb_set ; i++){
		m_table[i].resize(m_assoc);
		for(int j = 0 ; j < m_assoc ; j++){
			m_table[i][j] = new DirectoryEntry();
		}
	}
		
	m_policy = new DirectoryReplacementPolicy(m_assoc , m_nb_set , m_table);
	
}

Directory::~Directory()
{
	for(auto sets : m_table)
	{
		for(auto entry : sets)
			delete entry;
	}
	delete m_policy;
}


DirectoryEntry*
Directory::addEntry(uint64_t addr)
{
	if(getEntry(addr) != NULL)
		return getEntry(addr);	
	
	uint64_t id_set = indexFunction(addr);
	int assoc_victim = m_policy->evictPolicy(id_set);
	
	DirectoryEntry* entry = m_table[id_set][assoc_victim];
	entry->initEntry();
	entry->block_addr = addr;
	entry->isValid = true;
	
	m_policy->insertionPolicy(entry);
	return entry;
}

bool
Directory::lookup(uint64_t addr)
{
	
	uint64_t id_set = indexFunction(addr);
	for(unsigned i = 0 ; i < m_table[id_set].size() ; i++)
	{
		if(m_table[id_set][i]->block_addr == addr)
			return true;
	}	
	return false;
}

void 
Directory::setTrackerToEntry(uint64_t addr , int node)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);

	entry->nodeTrackers.clear();
	entry->nodeTrackers.insert(node);

	m_policy->updatePolicy(entry);
}



std::set<int>
Directory::getTrackers(uint64_t addr)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);
	return entry->nodeTrackers;
}
		

void 
Directory::removeTracker(uint64_t addr, int node)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);

	set<int>::iterator it = entry->nodeTrackers.find(node);
	if(it != entry->nodeTrackers.end())
		entry->nodeTrackers.erase(it);

	//the cl is no longer in a L1 cache
	if(entry->nodeTrackers.empty())
		entry->coherence_state = CLEAN_LLC;
}

void
Directory::setCoherenceState(uint64_t addr, DirectoryState dir_state)
{

	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);

//	entry->print();

	entry->coherence_state = dir_state;
	m_policy->updatePolicy(entry);
}


void
Directory::resetTrackersToEntry(uint64_t addr)
{
	DPRINTF("DIRECTORY::resetTrackersToEntry Addr %#lx\n", addr );

	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);

	entry->resetTrackers();	
	m_policy->updatePolicy(entry);
}


DirectoryEntry*
Directory::getEntry(uint64_t addr)
{
	int id_set = indexFunction(addr);
	for(auto entry : m_table[id_set])
	{
		if(entry->block_addr == addr)
			return entry;
	}
	return NULL;
}

void 
Directory::addTrackerToEntry(uint64_t addr, int node)
{
	DPRINTF("DIRECTORY::addTrackerToEntry Addr %#lx, node %d\n" , addr, node );

	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);	
	entry->nodeTrackers.insert(node);
	m_policy->updatePolicy(entry);
}

void
Directory::printConfig()
{
	cout << "Directory Configuration" << endl;
	cout << "\tAssociativity\t" << m_assoc << endl;
	cout << "\tNB Sets\t" << m_nb_set << endl;
}


void
Directory::printStats()
{
	cout << "Directory Statistique" << endl;
	cout << "********************" << endl;
}


uint64_t 
Directory::indexFunction(uint64_t addr)
{
	return addr % m_nb_set;
}


/******** Implementation of the replacement policy of the Directory */ 

DirectoryReplacementPolicy::DirectoryReplacementPolicy(int nbAssoc , int nbSet, 
			 std::vector<std::vector<DirectoryEntry*> >& dir_entries) : \
			 m_assoc(nbAssoc) , m_nb_set(nbSet), m_directory_entries(dir_entries)
{
	m_cpt = 1;
}

void
DirectoryReplacementPolicy::updatePolicy(DirectoryEntry* entry)
{
	m_cpt++;
	entry->policyInfo = m_cpt;
}

int 
DirectoryReplacementPolicy::evictPolicy(int set)
{
	int smallest_time = m_directory_entries[set][0]->policyInfo , smallest_index = 0;

	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(!m_directory_entries[set][i]->isValid) 
			return i;
	}
	
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(m_directory_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_directory_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}
	return smallest_index;
}


