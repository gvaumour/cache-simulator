#include <stdint.h>
#include <cmath>
#include <assert.h>
#include <stdlib.h> 
#include <string.h>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <bitset>

#include "common.hh"
#include "DBAMBPredictor.hh"
#include "Cache.hh"

using namespace std;

uint64_t cpt_time;

const char* memCmd_str[] = { "INST_READ", "INST_PREFETCH", "DATA_READ", "DATA_WRITE", "DATA_PREFETCH", "CLEAN_WRITEBACK", \
	"DIRTY_WRITEBACK", "SILENT_WRITEBACK", "INSERT", "EVICTION", "ACE"};

const char* str_allocDecision[] = {"ALLOCATE_IN_SRAM", "ALLOCATE_IN_NVM" , "BYPASS_CACHE", "ALLOCATE_PREEMPTIVELY"};


const char* directory_state_str[] = {"SHARED_L1" , "MODIFIED_L1", "EXCLUSIVE_L1", "SHARED_L1_NOT_LLC" ,\
			 "MODIFIED_L1_NOT_LLC" , "EXCLUSIVE_L1_NOT_LLC" , "CLEAN_LLC", "DIRTY_LLC" , "NOT_PRESENT"};

set<string> simulation_debugflags = {"DebugCache", "DebugDBAMB", "DebugHierarchy", "DebugFUcache" , "all"};	
	
const char* str_RW_status[] = {"DEAD" , "WO", "RO" , "RW", "RW_NOTACC"};
const char* str_RD_status[] = {"RD_SHORT" , "RD_MEDIUM", "RD_NOTACC", "UNKOWN"};


SimuParameters simu_parameters;
int miss_counter;

vector<string> 
split(string s, char delimiter){
  vector<string> result;
  std::istringstream ss( s );
  while (!ss.eof())
  {
		string field;
		getline( ss, field, delimiter );
		if (field.empty()) continue;
		result.push_back( field );
  }
  return result;
}

uint64_t 
bitSelect(uint64_t address , unsigned small , unsigned big)
{
	std::bitset<64> b(address);
	for(int i = big+1 ; i < 64 ; i++)
		b.reset(i);
	
	b >>= small;
	
	return b.to_ullong();	
}


uint64_t
bitRemove(uint64_t address , unsigned int small, unsigned int big){
    uint64_t mask;
    assert(big >= small);

	if (small == 0) {
		mask = (uint64_t)~0 << big;
		return (address & mask);
	} 
	else {
		mask = ~((uint64_t)~0 << small);
		uint64_t lower_bits = address & mask;
		mask = (uint64_t)~0 << (big + 1);
		uint64_t higher_bits = address & mask;

		higher_bits = higher_bits >> (big - small + 1);
		return (higher_bits | lower_bits);
	}
}

/** Fonction qui convertit une adresse en hexa de type "0x0774" en uint64_t */ 
uint64_t
hexToInt(string adresse_hex){
	char * p;
	uint64_t n = strtoul( adresse_hex.c_str(), & p, 16 ); 

	if ( * p != 0 ) {  
		cout << "not a number" << endl;
		return 0;
	}    
	else   
		return n;
}

static char * mydummy_var;

uint64_t
hexToInt1(const char* adresse_hex){
	uint64_t n = strtoul( adresse_hex, &mydummy_var, 16 ); 

	if ( *mydummy_var != 0 ) {  
		cout << "not a number" << endl;
		return 0;
	}    
	else   
		return n;
}


bool
readInputArgs(int argc , char* argv[] , int& sizeCache , int& assoc , int& blocksize, std::string& filename, std::string& policy)
{
	
	if(argc == 1){
		sizeCache = 512;
		assoc = 4;
		blocksize = 64;
		filename = "test.txt";		
		policy = "LRU";

	}
	else if(argc == 3){
		sizeCache = 16384;
		assoc = 4;
		blocksize = 64;
		filename = argv[1];	
		policy = argv[2];	
	}
	else if(argc == 6){
		sizeCache = atoi(argv[1]);
		assoc = atoi(argv[2]);
		blocksize = atoi(argv[3]);
		policy = argv[4];
		filename = argv[5];		
	}
	else{
		return false;
	}		
	return true;
}

string splitFilename(string str)
{
	size_t found=str.find_last_of("/\\");
	return str.substr(0 , found);
}


vector<string> splitAddr_Bytes(uint64_t a)
{
	stringstream ss;
	ss << std::hex << a;
   	string addr = ss.str();
	while(addr.size() < 16)
		addr = string(1,'0') + addr;

	vector<string> result;
	for(unsigned i = 0 ; i < addr.size(); i+=2)
	{
		stringstream ss1;
		ss1 << addr.at(i);
		ss1 << addr.at(i+1);
		result.push_back(ss1.str());
	}
	return result;
}

string buildHash(uint64_t a, uint64_t p)
{
	stringstream ss;
	ss << std::hex << a;
   	string addr = ss.str();

	while(addr.size() < 8)
		addr = string(1,'0') + addr;
			
	stringstream ss1;
	ss1 << std::hex << p;
   	string pc = ss1.str();	

	while(pc.size() < 8)
		pc = string(1,'0') + pc;

	return addr + pc;
}

bool			
isPow2(int x)
{ 
	return (x & (x-1)) == 0;
}


std::string
convert_hex(int n)
{
   std::stringstream ss;
   ss << std::hex << n;
   return ss.str();
}


const char *
StripPath(const char * path)
{
    const char * file = strrchr(path,'/');
    if (file)
        return file+1;
    else
        return path;
}

vector< vector<int> > 
resize_image(vector< vector<int> >& img)
{

	int width =img[0].size();
	vector<vector<int > > result = vector< vector<int> >(img.size() , vector<int>());
	int factor = ceil ( (double)width / (double) IMG_WIDTH );

	for(unsigned i = 0 ; i < img.size() ; i++)
	{
		for(unsigned j = 0 ; j < img[i].size() ; j+= factor)
		{
			int index = factor + j >= img[i].size() ? img[i].size() : factor+j; 
			int sum = 0;
			for(int k = j ; k < index; k++)
				sum += img[i][k];
				
//			cout << "result[" << i << "].push_back " << sum << " / " << factor << " = " <<  sum / factor << endl;
			result[i].push_back(sum / factor);
		}
	}	
	return result;
}

string
convertBool(bool a)
{
	if(a)
		return "TRUE";
	else
		return "FALSE";
}


void writeBMPimage(std::string image_name , int width , int height , vector< vector<int> > red, vector< vector<int> > blue, vector< vector<int> > green )
{
	FILE *f;
	int factor = 4;
	unsigned char *img = NULL;
	int w = width;
	int h = height*factor;
	int filesize = 54 + 3*w*h;

	img = (unsigned char *)malloc(3*w*h);
	memset(img,0,3*w*h);
	unsigned char r,g,b;
	int a = 0;
	
	for(int i=0; i<width; i++)
	{
	    a = 0;
	    for(int j=0; j<height; j++)
	    {
		r = red[i][j];
		g = green[i][j];
		b = blue[i][j];
		
		for(int k = 0 ; k < factor;k++)
		{
			img[(i+a*w)*3+2] = (unsigned char)(r);
			img[(i+a*w)*3+1] = (unsigned char)(g);
			img[(i+a*w)*3+0] = (unsigned char)(b);
			a++;
		}
	    }
	}

	unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
	unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
	unsigned char bmppad[3] = {0,0,0};

	bmpfileheader[ 2] = (unsigned char)(filesize    );
	bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
	bmpfileheader[ 4] = (unsigned char)(filesize>>16);
	bmpfileheader[ 5] = (unsigned char)(filesize>>24);

	bmpinfoheader[ 4] = (unsigned char)(       w    );
	bmpinfoheader[ 5] = (unsigned char)(       w>> 8);
	bmpinfoheader[ 6] = (unsigned char)(       w>>16);
	bmpinfoheader[ 7] = (unsigned char)(       w>>24);
	bmpinfoheader[ 8] = (unsigned char)(       h    );
	bmpinfoheader[ 9] = (unsigned char)(       h>> 8);
	bmpinfoheader[10] = (unsigned char)(       h>>16);
	bmpinfoheader[11] = (unsigned char)(       h>>24);

	f = fopen(image_name.c_str(),"wb");
	fwrite(bmpfileheader,1,14,f);
	fwrite(bmpinfoheader,1,40,f);
	for(int i=0; i<h; i++)
	{
	    fwrite(img+(w*(h-i-1)*3),3,w,f);
	    fwrite(bmppad,1,(4-(w*3)%4)%4,f);
	}

	free(img);
	fclose(f);
}

void
init_default_parameters()
{
	simu_parameters.enableBP = false;
	simu_parameters.enableMigration= false;
	simu_parameters.flagTest = true;	
	simu_parameters.printDebug = false;
		
	simu_parameters.prefetchDegree = 2;
	simu_parameters.prefetchStreams = 16; 
	simu_parameters.enablePrefetch = false;
	simu_parameters.strongInclusivity = false;
	
	simu_parameters.sram_assoc = 4;
	simu_parameters.nvm_assoc = 12;
	simu_parameters.nb_sets = 2048;
	
	simu_parameters.saturation_threshold = 2;	
	simu_parameters.cost_threshold = -5;
	
	simu_parameters.mediumrd_def = 4;

	simu_parameters.traceLLC = false;

	/********* Predictor Config *************/ 
	simu_parameters.nb_sampled_sets = 2048;
	simu_parameters.simulate_conflicts = false;
	simu_parameters.size_MT_NVMtags = 4;
	simu_parameters.size_MT_SRAMtags = 4;
	simu_parameters.MT_counter_th = 2;
	simu_parameters.MT_timeframe = 1E4;
	simu_parameters.nb_MTcouters_sampling = 1;
	simu_parameters.enableFullSRAMtraffic = false;

	/********* DBAMB Config *************/ 
	simu_parameters.window_size = 20; 
	simu_parameters.learningTH = 20;
	simu_parameters.deadSaturationCouter = 3;

	simu_parameters.rap_assoc = 128;
	simu_parameters.rap_sets = 128;
	
	simu_parameters.ratio_RWcost = -1;
		
	simu_parameters.readDatasetFile = false;
	simu_parameters.writeDatasetFile = false;
	simu_parameters.enableDatasetSpilling = false;
	simu_parameters.datasetFile = RAP_DATASET_FIRSTALLOC;

	simu_parameters.optTarget = EnergyParameters();
	simu_parameters.DBAMB_signature = "first_pc";
	
	simu_parameters.DBAMB_begin_bit = 0;
	simu_parameters.DBAMB_end_bit = 64;
	
	simu_parameters.enableReuseErrorComputation = false;
	simu_parameters.enablePCHistoryTracking = false;
	/***************************************/ 
	

	/********* Perceptron Config *************/ 	
//	simu_parameters.perceptron_features = { "Addr_LSB", "Addr_MSB", "PC_LSB", "PC_MSB"};
	simu_parameters.perceptron_BP_features = { "MissPC_0" , "Addr_2" };
	simu_parameters.perceptron_Allocation_features = { "MissPC_LSB"};
	simu_parameters.perceptron_windowSize = 16;
	
	simu_parameters.perceptron_bypass_threshold = 3;
	simu_parameters.perceptron_bypass_learning = 11;
	simu_parameters.perceptron_allocation_threshold = 5;
	simu_parameters.perceptron_allocation_learning = 11;

	simu_parameters.perceptron_compute_variance = false;
	simu_parameters.perceptron_table_size = 256;
	simu_parameters.perceptron_counter_size = 32;
	simu_parameters.perceptron_drawFeatureMaps = false;

	simu_parameters.perceptron_enableBypass = false;
	/***************************************/ 
	
	/*********** PHC Config ******************/ 
	simu_parameters.PHC_features = {"MissPC_0" , "Addr_3"};
	simu_parameters.PHC_cost_threshold = 0;
	simu_parameters.PHC_cost_mediumRead = -4124;
	simu_parameters.PHC_cost_mediumWrite = -3702;
	simu_parameters.PHC_cost_shortWrite = 405;
	simu_parameters.PHC_cost_shortRead = -35;

	/************ Cerebron Config ************/ 
	simu_parameters.Cerebron_activation_function = "linear";
	simu_parameters.Cerebron_resetEnergyValues = false;
	simu_parameters.Cerebron_separateLearning = false;
	simu_parameters.Cerebron_lowConfidence = false;
	simu_parameters.Cerebron_enableMigration = false;

	simu_parameters.Cerebron_fastlearning = true;
	simu_parameters.Cerebron_independantLearning = true;
	simu_parameters.Cerebron_RDmodel = true;

	/*
	simu_parameters.Cerebron_fastlearning = false;
	simu_parameters.Cerebron_independantLearning = false;
	simu_parameters.Cerebron_RDmodel = false;
	*/
	simu_parameters.Cerebron_decrement_value = 1;

	/************ SimplePerceptron Config *********/ 
	simu_parameters.simple_perceptron_features =  {"MissPC_0" , "Addr_3"};
	simu_parameters.simple_perceptron_learningTreshold = 5;
	simu_parameters.simple_perceptron_fastLearning = false;
	simu_parameters.simple_perceptron_independantLearning = false;
	simu_parameters.simple_perceptron_enableMigration = false;


	simu_parameters.nbCores = 1;
	simu_parameters.policy = "SimplePerceptron";

}


