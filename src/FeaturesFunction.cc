#include "common.hh"
#include "FeaturesFunction.hh"

using namespace std;


int hashingAddr_LSB1(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return ( bitSelect(addr , 18 , 25)^pc_history[0]) % 256;
}

int hashingAddr_LSB(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return bitSelect(addr , 18 , 25);
}

int hashingcurrentPC_1(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return ((pc_history[1]>>1)^pc_history[0])%256;
}

int hashingcurrentPC_2(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return ((pc_history[2]>>2)^pc_history[0])%256;
}

int hashingcurrentPC_3(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return ((pc_history[3]>>3)^pc_history[0])%256;
}


int hashingTag_block(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	uint64_t tag = bitSelect(addr , 18 , 64);
	return ((tag >> 4)^pc_history[0])%256;
}

int hashingTag_page(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	uint64_t tag = bitSelect(addr , 18 , 64);
	return ((tag >> 7)^pc_history[0])%256;
}


int
hashingMissPC_LSB(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return bitSelect(missPC , 0 , 7);
}

int
hashingMissPC_LSB1(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return ( bitSelect(missPC , 0 , 7)^pc_history[0])%256;
}


int
hashingMissPC_MSB(uint64_t addr , uint64_t missPC , deque<uint64_t> pc_history)
{
	return bitSelect(missPC , 20 , 27);
}

int 
hashing_MissCounter(uint64_t addr, uint64_t missPC, deque<uint64_t> pc_history)
{
	return miss_counter;
}

int 
hashing_MissCounter1(uint64_t addr, uint64_t missPC, deque<uint64_t> pc_history)
{
	return (miss_counter^pc_history[0])%256;
}

int 
hashing_currentPC(uint64_t addr, uint64_t missPC, deque<uint64_t> pc_history)
{
	return (pc_history[0])%256;
}

int 
hashing_currentPC1(uint64_t addr, uint64_t missPC, deque<uint64_t> pc_history)
{
	return ( (pc_history[0]>>2)^pc_history[0])%256;
}


