import re
import sys

def usage(): 
	print("Usage:");
	print("python ./energy_model.py <config_file> <result_file>");

if len(sys.argv) != 3:
	print("Wrong input parameters");
	usage();
	exit();
			
config_file = sys.argv[1];
results_file = sys.argv[2];

########################################################
#The values taken here come from NVSim output simulation 
#available in the directoy nvsim/my_output
READ_ACCESS=0;
WRITE_ACCESS=1;
LEAKAGE=2;	

costTag = [0,0,0];
costTag[READ_ACCESS] = 28.679E-12;
costTag[WRITE_ACCESS] = 28.125E-12;
costTag[LEAKAGE] = 244.304E-3;

costSRAM = [0,0,0];
costSRAM[READ_ACCESS] = 271.678E-12;
costSRAM[WRITE_ACCESS] = 257.436E-12;
costSRAM[LEAKAGE] = 2.587;

costNVM = [0,0,0];
costNVM[READ_ACCESS] = 236.828E-12;
costNVM[WRITE_ACCESS] = 652.278E-12;
costNVM[LEAKAGE] = 494.545E-3;

costDRAMRead = 4.08893E-9;
costDRAMWrite = 4.10241E-9;
########################################################

#Cost of the DHPTable
#map<int,map<int,double> > cost_DHPTable = { { 2 , { { 32, 2}, { 64, 4}, { 128 , 4} } }, \
#	{ 4 , { { 32, 4}, { 64, 6}, { 128 , 7} } }, \
#	{ 8 , { { 32, 4}, { 64, 9}, { 128 , 13} } }, \
#	{ 128 , { { 32, 4}, { 64, 9}, { 128 , 13} } } };
########################################################
 
predictor="";
ratio=-1;

with open(config_file) as f:
	for line in f:
	
		m = re.search('CacheLLC:Predictor\s([A-Z]+\w+)', line);
		if m != None:
			predictor = m.group(1);
		m = re.search('CacheLLC:SRAMways\s(\d+)', line);
		if m != None:
			sram_ways = int(m.group(1));
		m = re.search('CacheLLC:NVMways\s(\d+)', line);
		if m != None:
			nvm_ways = int(m.group(1));
		m = re.search('CacheLLC:DBAMBPredictor:RAPTableAssoc\s(\d+)', line);
		if m != None:
			dhp_table_assoc = int(m.group(1));
		m = re.search('CacheLLC:DBAMBPredictor:RAPTableNBSets\s(\d+)', line);
		if m != None:
			dhp_table_sets = int(m.group(1));
		m = re.search('CacheLLC:DBAMBPredictor:CostRWRatio\s(\d+)', line);
		if m != None:
			ratio = int(m.group(1));

if ratio != -1:
	costNVM[WRITE_ACCESS] = ratio * costNVM[READ_ACCESS];

DRAM_read,DRAM_write = 0,0;
nbFromSRAMmigration,nbFromNVMmigration = 0,0
nbSRAMWrite, nbSRAMRead, nbNVMWrite, nbNVMRead = 0,0,0,0

with open(results_file) as f:
	for line in f:
	
		m = re.search('CacheLLC:NVMways:reads\s(\d+)', line);
		if m != None:
			nbNVMRead = int(m.group(1));
		m = re.search('CacheLLC:NVMways:writes\s(\d+)', line);
		if m != None:
			nbNVMWrite = int(m.group(1));
		m = re.search('CacheLLC:SRAMways:reads\s(\d+)', line);
		if m != None:
			nbSRAMRead = int(m.group(1));
		m = re.search('CacheLLC:SRAMways:writes\s(\d+)', line);
		if m != None:
			nbSRAMWrite = int(m.group(1));
		m = re.search('CacheLLC:MigrationFromNVM\s(\d+)', line);
		if m != None:
			nbFromNVMmigration = int(m.group(1));
		m = re.search('CacheLLC:nbFromSRAMmigration\s(\d+)', line);
		if m != None:
			nbFromSRAMmigration = int(m.group(1));
		m = re.search('MainMem:ReadFetch\s(\d+)', line);
		if m != None:
			DRAM_read += int(m.group(1));
		m = re.search('MainMem:WriteFetch\s(\d+)', line);
		if m != None:
			DRAM_write += int(m.group(1));
		m = re.search('MainMem:DirtyWB\s(\d+)', line);
		if m != None:
			DRAM_write += int(m.group(1));
		m = re.search('CacheLLC:Bypass\s(\d+)', line);
		if m != None:
			nbBypass = int(m.group(1));
		m = re.search('CacheLLC:Predictor:SRAMError\s(\d+)', line);
		if m != None:
			nbSRAMErrors = int(m.group(1));


##All infos are collected for the files
##Go to the computation of the consumption of the LLC 

NVMconsoRead, SRAMconsoRead = 0, 0;
NVMconsoWrite, SRAMconsoWrite = 0, 0;
	
NVMconso, SRAMconso, memoryConso, totalConso = 0,0,0,0;
nvsp_tableConso = 0;

if nvm_ways != 0:
	#Cost of the NVM array
	NVMconsoWrite = (nbNVMWrite - nbFromSRAMmigration) * costNVM[WRITE_ACCESS];
	NVMconsoRead = (nbNVMRead - nbFromNVMmigration) * costNVM[READ_ACCESS];
	NVMconso = NVMconsoRead + NVMconsoWrite;
	
	
#Cost of the SRAM array
SRAMconsoRead =  (nbSRAMRead - nbFromSRAMmigration) * costSRAM[READ_ACCESS];
SRAMconsoWrite = (nbSRAMWrite - nbFromNVMmigration) * costSRAM[WRITE_ACCESS];
SRAMconso = SRAMconsoRead + SRAMconsoWrite;

#Cost of the Main Memory
memoryConso = costDRAMRead * (DRAM_read + nbBypass);
memoryConso += costDRAMWrite * (DRAM_write); 

#nvsp_tableConso = RAPTableCost * (nbSRAMRead + nbSRAMWrite + nbNVMRead + nbNVMWrite + nbBypass);
nvsp_tableConso = 0;

#Cost of Migration computed separately
fromNVMmigration = nbFromNVMmigration * costNVM[READ_ACCESS] + costSRAM[WRITE_ACCESS];
fromSRAMmigration =  nbFromSRAMmigration * costSRAM[READ_ACCESS] + costNVM[WRITE_ACCESS];
costMigration =  fromNVMmigration + fromSRAMmigration;		

totalConso = SRAMconso + NVMconso + memoryConso + nvsp_tableConso + costMigration;
  
print "Dynamic Conso\t" + str(totalConso) 
print "\tSRAM Read\t" + '%1.3e' % (SRAMconsoRead) + "\t" + '%02.1f' % (SRAMconsoRead*100/totalConso) + "%" 
print "\tSRAM Write\t" + '%1.3e' % (SRAMconsoWrite) + "\t" + '%02.1f' % (SRAMconsoWrite*100/totalConso) + "%" 
print "\tNVM Read\t"  + '%1.3e' % (NVMconsoRead) + "\t" + '%02.1f' % (NVMconsoRead*100/totalConso) + "%" 
print "\tNVM Write\t"  + '%1.3e' % (NVMconsoWrite) + "\t" + '%02.1f' % (NVMconsoWrite*100/totalConso) + "%" 
print "\tMigration\t"  + '%1.3e' % (costMigration) + "\t" + '%02.1f' % (costMigration*100/totalConso) + "%" 
print "\tNVSPTable\t"  + '%1.3e' % (nvsp_tableConso) + "\t"+ '%02.1f' % (nvsp_tableConso*100/totalConso) + "%" 
print "\tMainMemo\t"  + '%1.3e' % (memoryConso) + "\t" + '%02.1f' % (memoryConso*100/totalConso) + "%" 
	
costNVMerror =  nbNVMWrite * costNVM[WRITE_ACCESS];
costSRAMerror =  nbSRAMErrors * DRAM_read;
	
print "**********" 
print "Relative Cost Errors :" 
print "Error NVM\t"+ '%1.3e' % (costNVMerror) + "\t" + '%02.1f' % (costNVMerror*100/totalConso) + "%" 
print "Error SRAM\t" + '%1.3e' %(costSRAMerror) + "\t" + '%02.1f' % (costSRAMerror*100/totalConso) + "%" 

#Compute the cost of the migration overhead
if predictor == "DBAM" or predictor == "Saturation":
	print "**********" 
	print "Migration cost\t" + str(costMigration) + "\t" + str(costMigration*100/totalConso) + "%"
	print "Decomposed as:" 
	print "\tFrom NVM\t" + '%1.3e' % (fromNVMmigration) + "\t" + str(fromNVMmigration*100/costMigration)
	print "\tFrom SRAM\t" + '%1.3e' % (fromSRAMmigration) + "\t" + str(fromSRAMmigration*100/costMigration)
	 
