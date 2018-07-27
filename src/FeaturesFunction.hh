#ifndef __FEATURES__FUNCTION__HH__
#define __FEATURES__FUNCTION__HH__

#include <stdint.h>
#include <deque>

typedef int (*hashing_function)(uint64_t , uint64_t); 

class params_hash
{
	public: 
		params_hash(std::string a, bool b, int c)
		{
			index = a;
			xorWithPC = b;
			nbBlock = c;
		};
		params_hash() : params_hash("" , false, 0){};
	
		std::string index;
		bool xorWithPC;
		int nbBlock; //8bits for the index to take 
};


int hashing_function1(params_hash a , uint64_t addr , uint64_t missPC);

int hashingAddr_MSB(uint64_t addr , uint64_t missPC );
int hashingAddr_LSB(uint64_t addr , uint64_t missPC );
int hashingAddr_LSB1(uint64_t addr , uint64_t missPC );

int hashingTag_block(uint64_t addr , uint64_t missPC );
int hashingTag_page(uint64_t addr , uint64_t missPC );


int hashingMissPC_MSB(uint64_t addr , uint64_t missPC );
int hashingMissPC_LSB(uint64_t addr , uint64_t missPC );
int hashingMissPC_LSB1(uint64_t addr , uint64_t missPC );

int hashing_MissCounter(uint64_t addr, uint64_t missPC);
int hashing_MissCounter1(uint64_t addr, uint64_t missPC);

int hashing_CallStack(uint64_t addr, uint64_t missPC);
int hashing_CallStack1(uint64_t addr, uint64_t missPC);

int hashing_currentPC(uint64_t addr, uint64_t missPC);
int hashing_currentPC1(uint64_t addr, uint64_t missPC);
int hashingcurrentPC_3(uint64_t addr, uint64_t missPC);
int hashingcurrentPC_2(uint64_t addr, uint64_t missPC);
int hashingcurrentPC_1(uint64_t addr, uint64_t missPC);

#endif
