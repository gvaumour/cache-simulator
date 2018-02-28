import os
import re
import sys

timing_values = { 'LLC' : { 'SRAM' : {'reads' : 15 , 'writes' : 15}, 'NVM' : {'reads' : 15, 'writes' : 70}, 'Time' : 0}, \
		  'L1I' : {'SRAM' : {'reads' : 1 , 'writes' : 1} , 'NVM' : {'reads' : 2, 'writes' : 2} , 'Time' : 0}, \
		  'L1D' :  {'SRAM' : {'reads' : 1 , 'writes' : 1} , 'NVM' : {'reads' : 2, 'writes' : 2}, 'Time' : 0}, \
	 	  'DRAM' : {'Read' : 100 , 'Write' : 100 , 'Time' : 0} };
	 
	 
frequency = 1E9;

leakage_power = { 
	"0:27" : 11.00, \
	"1:26" : 13.84, \
	"2:24" : 15.21, \
	"3:21" : 18.51, \
	"4:12" : 11.00, \
	"4:20" : 24.74, \
	"5:18" : 27.56, \
	"6:16" : 30.38, \
	"7:15" : 33.72, \
	"8:13" : 36.64, \
	"9:12" : 39.06, \
	"10:10" : 41.06, \
	"11:8" : 43.07, \
	"12:7" : 45.84, \
	"13:5" : 48.54, \
	"14:3" : 50.46, \
	"15:2" : 52.44, \
	"16:0" : 53.18, \
	"8:300" : 13.21, \
	"16:300" : 13.21, \
	"32:300" : 13.21, \
}

if len(sys.argv) != 3:
	print("Wrong input parameters");
	usage();
	exit();
			
config_file = sys.argv[1];
results_file = sys.argv[2];


nbSRAMways, nbNVMways = -1, -1;

with open(config_file) as f:
	for line in f:
	
		m = re.search('CacheLLC:Predictor\s([A-Z]+\w+)', line);
		if m != None:
			predictor = m.group(1);
		m = re.search('CacheLLC:SRAMways\s(\d+)', line);
		if m != None:
			nbSRAMways = int(m.group(1));
		m = re.search('CacheLLC:NVMways\s(\d+)', line);
		if m != None:
			nbNVMways = int(m.group(1));


with open(results_file) as f:

	for line in f:
		
		m = re.search('Cache(L1D|L1I|LLC):(SRAM|NVM)ways:(reads|writes)\s(\d+)', line);
		
	
		if m != None:
			field1 = m.group(1);
			field2 = m.group(2);
			field3 = m.group(3);
			value = m.group(4);			
			timing_values[field1]['Time'] += int(timing_values[field1][field2][field3]) * int(value);		

		m = re.search('MainMem:(Read|Write)Fetch\s(\d+)', line);
		if m != None:
			field1 = m.group(1);
			value = m.group(2);			
			timing_values['DRAM']['Time'] += int(timing_values['DRAM'][field1]) * int(value);

		m = re.search('MainMem:DirtyWB\s(\d+)', line);
		if m != None:
			value = m.group(1);
			timing_values['DRAM']['Time'] += int(timing_values['DRAM']['Write']) * int(value);

total_timing = 0;
for k,v in timing_values.items():
	total_timing += timing_values[k]['Time'];


print("Total Time\t" + str(total_timing));
for k,v in timing_values.items():
	print("\t" + k + "\t" + str(timing_values[k]['Time']) + "\t" + str(100*timing_values[k]['Time']/total_timing) + "%");

key = str(nbSRAMways) + ":" + str(nbNVMways);
leakage = total_timing * leakage_power[key]*(1E-4)/3 / frequency;

print("Leakage Power\t" + '%1.3e' % (leakage)); 

