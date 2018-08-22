#include "common.hh"
#include "Perceptron.hh"
#include "Cerebron.hh"
#include "FeaturesFunction.hh"


using namespace std;


int hashing_function1(params_hash a , uint64_t addr , uint64_t missPC)
{
	string index = a.index;
	bool hash_with_PC = a.xorWithPC;
	int block = a.nbBlock;
	//string b = hash_with_PC ? "TRUE" : "FALSE";
	//cout << "index = " << index << " block = " << block << " hash_with_PC = " << b << endl;
	assert( block < 8);
	uint64_t tag = 0;
	
	if( index == "MissPC")
		tag = bitSelect( missPC , 8*block , 8*block+7);
	else if( index == "Addr")
		tag = bitSelect( addr , 8*block , 8*block+7);
	else if( index == "currentPC")
		tag = bitSelect( CerebronPredictor::m_global_PChistory[0] , 8*block , 8*block+7);
	else if( index == "currentPC1")
		tag = bitSelect( CerebronPredictor::m_global_PChistory[1] , 8*block , 8*block+7);
	else if( index == "currentPC2")
		tag = bitSelect( CerebronPredictor::m_global_PChistory[2] , 8*block , 8*block+7);
	else if( index == "currentPC3")
		tag = bitSelect( CerebronPredictor::m_global_PChistory[3] , 8*block , 8*block+7);
	/*else if( index == "MissCounter")
	{
	
	}*/
	else
		assert(false && "Hashing Function failed");
	
	if( hash_with_PC)
		tag = (tag ^ CerebronPredictor::m_global_PChistory[0])%256;

	return tag;
}

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
	uint64_t tag = bitSelect(addr , 16 , 23);
//	return ((tag >> 4)^PerceptronPredictor::m_global_PChistory[0])%256;
//	return ((tag >> 4)%256);
	return tag;
}

int hashingTag_page(uint64_t addr , uint64_t missPC )
{
	uint64_t tag = bitSelect(addr , 24 , 31);

//	cout << "Addr = " << std::hex << addr << " hash = " << tag << endl;

//	return ((tag >> 7)^PerceptronPredictor::m_global_PChistory[0])%256;
//	return ((tag >> 7)%256);
	return tag;
}


int
hashingMissPC(uint64_t addr , uint64_t missPC, int block, bool hash_with_PC)
{
	assert( block < 8);
	
	uint64_t tag = bitSelect( missPC , 8*block , 8*block+7);
	if( hash_with_PC)
		tag = (tag ^ PerceptronPredictor::m_global_PChistory[0])%256;

	return tag;
}

int
hashingAddr(uint64_t addr , uint64_t missPC, int block, bool hash_with_PC)
{
	assert( block < 8);
	
	uint64_t tag = bitSelect( addr , 8*block , 8*block+7);
	if( hash_with_PC)
		tag = (tag ^ PerceptronPredictor::m_global_PChistory[0])%256;

	return tag;
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


