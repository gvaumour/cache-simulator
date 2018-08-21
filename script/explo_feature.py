import subprocess
import re
import sys
import os.path

if len(sys.argv) < 2:
	trace = "/home/gvaumour/Dev/apps/pin/cache-simulator/test/streaming_trace.txt"
else:
	trace = sys.argv[1];

features = [];

#available_features = ["Addr_0", "Addr_1" , "Addr_2","Addr_3", "Addr_4" , "Addr_5" , "MissPC_0" , "MissPC_1" , "MissPC_2" , "MissPC_3" , "MissPC_4" , "MissPC_5" , "currentPC_0", "currentPC_1", "currentPC_2", "currentPC_3", "currentPC_4", "currentPC_5" ];

available_features = [];

for i in range(6):
	available_features.append("Addr_" + str(i));
	available_features.append("MissPC_" + str(i));
	available_features.append("currentPC_" + str(i));
	available_features.append("currentPC1_" + str(i));
	available_features.append("currentPC2_" + str(i));
	available_features.append("Addr_" + str(i) + "_1");
	available_features.append("MissPC_" + str(i) + "_1");
	available_features.append("currentPC1_" + str(i) + "_1");
	available_features.append("currentPC2_" + str(i) + "_1");

pc_name=subprocess.check_output("hostname -f", shell=True);
if pc_name == "gvaumour-HP-EliteBook-840-G3\n":
	working_dir =  "/home/gvaumour/Dev/apps/pin/cache-simulator/"
else:
	working_dir="/home/gvaumour/proj/gvaumour/pin/cache-simulator/";
	
simulator= working_dir + "obj-intel64/roeval_release";
launch_energy = working_dir + "utils/others/launch_energy.sh";

best_energy = 1;
best_features = "";
results = {};

current_dir = os.getcwd();

for i in range(4):
	
	os.chdir(current_dir);

	a = str(i+1) + "features";
	if not os.path.exists(a):
		os.makedirs(a);
	os.chdir(a);
	
	current_best_energy = best_energy;
	

	for current_feature in available_features:

		if current_feature in features:
			continue;

		
		cmd = simulator + " -p Cerebron --Cerebron-fastLearning --Cerebron-independantLearning ";
		cmd += "--PHC-Features ";
		features_str = "";
		for feat in features:
			features_str +=feat+",";
		features_str +=current_feature;

		if not os.path.exists(features_str):
			os.makedirs(features_str);
		os.chdir(features_str);
	
		cmd += features_str + " ";
		cmd += trace;
		subprocess.check_call(cmd , shell=True);
	
	
		subprocess.check_call(launch_energy , shell="True");
	
		energy_file = open("energy.out", "r");
		line = energy_file.readline();
		energy_file.close();
	
		if line == '':
			print "Error in computing the energy model";
			continue;
	
		split_line = line.split("\t");
		energy = float(split_line[1]);
		
		results[features_str] = energy;	
	
		if energy < best_energy:
			best_energy = energy;
			best_features = features_str;
			best_feature = current_feature;

		os.chdir(current_dir+"/"+a);
		
	if current_best_energy <= best_energy:
		print "Stop exploring with i=" + str(i);
		break;
	else:
		print "Best feature is " + best_feature;
		features.append(best_feature);
		
		
print results;
print "Best set of Features is " + best_features + " , Energy = " + str(best_energy);
