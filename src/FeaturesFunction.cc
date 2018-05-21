#include "common.hh"
#include "Perceptron.hh"
#include "FeaturesFunction.hh"


using namespace std;

int hashingAddr_LSB1(uint64_t addr , uint64_t missPC )
{
	return ( bitSelect(addr , 18 , 25)^PerceptronPredictor::m_global_PChistory[0]) % 256;
}

int hashingAddr_LSB(uint64_t addr , uint64_t missPC )
{
	return bitSelect(addr , 18 , 25);
}

int hashingcurrentPC_1(uint64_t addr , uint64_t missPC )
{
	return ((PerceptronPredictor::m_global_PChistory[1]>>1)^PerceptronPredictor::m_global_PChistory[0])%256;
}

int hashingcurrentPC_2(uint64_t addr , uint64_t missPC )
{
	return ((PerceptronPredictor::m_global_PChistory[2]>>2)^PerceptronPredictor::m_global_PChistory[0])%256;
}

int hashingcurrentPC_3(uint64_t addr , uint64_t missPC )
{
	return ((PerceptronPredictor::m_global_PChistory[3]>>3)^PerceptronPredictor::m_global_PChistory[0])%256;
}


int hashingTag_block(uint64_t addr , uint64_t missPC )
{
	uint64_t tag = bitSelect(addr , 18 , 64);
//	return ((tag >> 4)^PerceptronPredictor::m_global_PChistory[0])%256;
	return ((tag >> 4)%256);
}

int hashingTag_page(uint64_t addr , uint64_t missPC )
{
	uint64_t tag = bitSelect(addr , 18 , 64);
//	return ((tag >> 7)^PerceptronPredictor::m_global_PChistory[0])%256;
	return ((tag >> 7)%256);
}


int
hashingMissPC_LSB(uint64_t addr , uint64_t missPC )
{
	return bitSelect(missPC , 0 , 7);
}

int
hashingMissPC_LSB1(uint64_t addr , uint64_t missPC )
{
	return ( bitSelect(missPC , 0 , 7)^PerceptronPredictor::m_global_PChistory[0])%256;
}


int
hashingMissPC_MSB(uint64_t addr , uint64_t missPC )
{
	return bitSelect(missPC , 20 , 27);
}

int 
hashing_MissCounter(uint64_t addr, uint64_t missPC)
{
	return miss_counter;
}

int 
hashing_MissCounter1(uint64_t addr, uint64_t missPC)
{
	return (miss_counter^PerceptronPredictor::m_global_PChistory[0])%256;
}

int 
hashing_currentPC(uint64_t addr, uint64_t missPC)
{
	return (PerceptronPredictor::m_global_PChistory[0])%256;
}

int 
hashing_currentPC1(uint64_t addr, uint64_t missPC)
{
	return ( (PerceptronPredictor::m_global_PChistory[0]>>2)^PerceptronPredictor::m_global_PChistory[0])%256;
}

int hashing_CallStack(uint64_t addr, uint64_t missPC )
{
	return (PerceptronPredictor::m_callee_PChistory[0])%256;
}

int hashing_CallStack1(uint64_t addr, uint64_t missP )
{
	return ((PerceptronPredictor::m_callee_PChistory[0])^PerceptronPredictor::m_global_PChistory[0])%256;
}


