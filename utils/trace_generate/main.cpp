#include <fstream>
#include <vector>
#include <iostream>
#include <string>
#include <math.h>
#include <zlib.h>

#define LINE_SIZE 50

using namespace std;

char buffer[LINE_SIZE];

void writing_access(uint64_t addr, int type , uint64_t pc , gzFile trace, ofstream& text_trace, bool isCompressed);

int main(int argc , char* argv[])
{
	bool isCompressed = false;
	if(argc > 1)
	{
		for(int i = 0 ; i < argc ; i++)
		{
			string param = string(argv[i]);
			if(param  == "-c")
				isCompressed = true;
		}
	}
	srand (time(NULL));

	gzFile trace;
	ofstream text_trace;
	if( isCompressed )
		trace = gzopen("test_trace.out" , "wb8");	
	else
		text_trace.open("text_trace.txt");
	

	uint64_t missPC = 0x004;
	
	for(int i = 0 ; i < 1000000 ; i++)
	{
		uint64_t lsbs = rand() % (65536);
		uint64_t addr1 = lsbs + pow (2, 24);
		uint64_t addr2 = lsbs + pow (2, 25);
		
//		int token = rand() % 100;
//		int type = token > 50 ? 1 : 2; 
		
		writing_access(addr1 , 1 , missPC , trace , text_trace , isCompressed);
		
		writing_access(addr2 , 2 , missPC , trace , text_trace , isCompressed);
	}
	
	
	if( isCompressed )
		gzclose(trace);
	else
		text_trace.close();

	return 0;
}

void writing_access(uint64_t addr, int type , uint64_t pc , gzFile trace, ofstream& text_trace, bool isCompressed)
{
	sprintf(buffer, "#!0x%lx %d 4 0 0x%lx" , addr , type , pc);
		
	if( isCompressed )			
		gzwrite(trace, buffer, LINE_SIZE);
	else
		text_trace << buffer << endl;

}
