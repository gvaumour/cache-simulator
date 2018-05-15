#ifndef __FEATURES__FUNCTION__HH__
#define __FEATURES__FUNCTION__HH__

#include <stdint.h>
#include <deque>


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
