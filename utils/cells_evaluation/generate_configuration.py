from subprocess import call
from scipy.interpolate import interp1d
import numpy as np
import sys
import re
import os

def simulate_cache_config(STTmemory_cell, output_dir):
#	call(["./simulate.sh", STTmemory_cell, output_dir], stdout=str(output_dir + "/nvsim_simu.out"));
	call(["./nvsim_simulation.sh", STTmemory_cell, output_dir]);

#Generate the amat_computation.py script and 
#launch_energy.sh script for energy modeling
def generate_energyScripts( output_dir , leakage_powers):

	#Generate amat_computation.py from the existing template 
	with open('./amat_computation_template.py', 'r') as template_amat_script:
    		template_amat = template_amat_script.read()
	
	template_amat = template_amat.replace('leakage_power=', "leakage_power= " + str(leakage_powers) );

	generated_amat_file = os.path.abspath(str(output_dir) + "/amat_computation.py");
	with open(generated_amat_file, 'w') as amat_script:
		amat_script.write(template_amat);
		
	#Generate launch_energy.sh from the existing template 
	with open('./launch_energy_template.sh', 'r') as template_launch_energy_script:
    		template_launch_energy = template_launch_energy_script.read()
    		
	template_launch_energy= template_launch_energy.replace("AMAT=", "AMAT="+ str(generated_amat_file) );

	generated_launch_file = os.path.abspath(str(output_dir) + "/launch_energy.sh");
	with open(generated_launch_file, 'w') as launch_energy_script:
		launch_energy_script.write(template_launch_energy);
	
	#####################
	
#Generate the simulation with the configuration for the NVM and SRAM assocs 
def generate_simulationScript( output_dir , assoc_configs , nb_sets):
	with open('./simulation_script_template.sh', 'r') as template_simulation_script:
    		template_simulation = template_simulation_script.read()
	
	template_simulation = template_simulation.replace("declare -a sram_assocs=" , "declare -a sram_assocs=(" + str(assoc_configs[1]) + ");");
	template_simulation = template_simulation.replace("declare -a nvm_assocs=" , "declare -a nvm_assocs=(" + str(assoc_configs[0]) + ");");
	template_simulation = template_simulation.replace("nb_sets=" , "nb_sets=" + str(nb_sets));
	template_simulation = template_simulation.replace("current=" , "current=" + str(os.path.abspath(output_dir)));
	
	generated_simulation_file = os.path.abspath( str(output_dir) + "/simulate.sh");
	with open(generated_simulation_file, 'w') as simulation_script:
		simulation_script.write(template_simulation);
	
	
def getArea(isNVM, size):

	techno ="SRAM"
	if isNVM:
		techno = "sample_STTRAM"
	if size == 0:
		return 0,0;
	
	file_to_read = output_dir + "/nvsim_output/" + techno + "_"+str(size)+"KB.out";
	leakage,area = 0, 0;
	with open(file_to_read) as f:
		for line in f:
			
			m = re.search('Total Area = (\d\.\d+)' , line);		
			if m != None:
				area = float(m.group(1));

			m = re.search("Cache Total Leakage Power\s+=\s+(\d+\.\d+)", line);
			if m != None:
				leakage = float(m.group(1));
				break;
	return area, leakage

print sys.argv;
if len(sys.argv) != 3:
	print "Need the memory cell and an ouput dir";
	sys.exit();

STTmemory_cell=os.path.abspath(sys.argv[1]);
output_dir=sys.argv[2];
simulate_cache_config(STTmemory_cell , output_dir);

size_full_SRAM=2048; #in KBytes
nb_sets= 2048;
area_ref, leakage_ref = getArea(False, size_full_SRAM); 

area_nvms_ref = [];
leakage_nvms_ref = [];
area_sram_ref = [];
leakage_sram_ref = [];
size_refs = [];

iter_size=32;
for a in range(0,9):
	area_NVM, leakage_NVM = getArea(True, iter_size);
	area_SRAM, leakage_SRAM = getArea(False, iter_size);

	size_refs.append(iter_size);
	
	area_nvms_ref.append(area_NVM);
	leakage_nvms_ref.append(leakage_NVM);
	
	area_sram_ref.append(area_SRAM);
	leakage_sram_ref.append(leakage_SRAM);
	
	iter_size = iter_size * 2;

area_nvms_ref = np.array( area_nvms_ref );
leakage_nvms_ref = np.array( leakage_nvms_ref );
area_sram_ref = np.array( area_sram_ref );
leakage_sram_ref = np.array( leakage_sram_ref );
size_refs = np.asarray( size_refs );


print size_refs;
print leakage_nvms_ref;
print area_nvms_ref;

interpolate_SRAMarea = interp1d(size_refs , area_sram_ref, kind=1);
interpolate_SRAMleakage = interp1d(size_refs , leakage_sram_ref, kind=1);

interpolate_NVMarea = interp1d(size_refs , area_nvms_ref)
interpolate_NVMarea_reverse = interp1d(area_nvms_ref, size_refs);

interpolate_NVMleakage = interp1d(size_refs, leakage_nvms_ref);

#import matplotlib.pyplot as plt
#xnew = np.linspace(128, 8000, num=100, endpoint=True)
#plt.plot(size_refs, area_sram_ref, 'o', xnew , interpolate_SRAMarea(xnew), '-')
#plt.legend(['data', 'linear'], loc='best')
#plt.show()


summary_file = open( output_dir + "/summary_file.txt", 'w');
#simulation_script = open( output_dir + "/memorycell_explo", 'w');

summary_file.write("SRAMCacheRef\t" + str(size_full_SRAM) + "KB\n");
summary_file.write("NBSets\t" + str(nb_sets) + "\n");
summary_file.write("AreaRef\t" + str(area_ref) + "mm2\n");
summary_file.write("LeakageRef\t" + str(leakage_ref) + "mW\n\n");

summary_file.write("SizeSRAM\tAssocSRAM\tAreaSRAM\tSizeNVM\tAssocNVM\tAreaNVM\tErrorArea\n")

leakage_powers = {};
assocs = [ "" , ""]

for assoc_SRAM in range(0,17):

	size_SRAM = assoc_SRAM * nb_sets * 64 / 2**10; # Size of SRAM in KB 
	if size_SRAM == 0:
		area_SRAM = 0;
		leakage_SRAM = 0;
	else:
		area_SRAM = interpolate_SRAMarea(size_SRAM);
	
	area_left = area_ref - area_SRAM;
	
	if area_left < area_nvms_ref[0]:
		size_NVM, area_NVM, assoc_NVM = 0 , 0 ,0;
	elif area_left > area_nvms_ref[-1]:
		size_NVM = size_refs[-1];
		area_NVM = area_nvms_ref[-1];
		assoc_NVM = int(round( size_NVM * 2**10 / (nb_sets * 64)));
	else:		
		size_NVM = interpolate_NVMarea_reverse(area_left)
		assoc_NVM = int(round( size_NVM * 2**10 / (nb_sets * 64)))
		size_NVM = int(assoc_NVM * 64 * nb_sets / 2**10);	
		area_NVM = interpolate_NVMarea(size_NVM);
		
	error_area = ( 1 - (area_SRAM + area_NVM) / area_ref ) *100;
	
	summary_file.write( str(size_SRAM) + "\t" + str(assoc_SRAM) + "\t" + '%1.3f' % area_SRAM \
	+ "\t" + str(size_NVM) + "\t" + str(assoc_NVM) + "\t" + '%1.3f' % area_NVM + "\t" + '%2.1f' % error_area+"%\n");


	leakage_NVM = interpolate_NVMleakage(size_NVM) if size_NVM > size_refs[0] else 0;
	leakage_SRAM = interpolate_SRAMleakage(size_SRAM) if size_SRAM > size_refs[0] else 0	 
	leakage_powers[str(assoc_SRAM) + ":" + str(assoc_NVM)] = leakage_NVM + leakage_SRAM;

	assocs[0] += " " + str(assoc_NVM);
	assocs[1] += " " + str(assoc_SRAM);
	
summary_file.close();

print leakage_powers;
generate_energyScripts(output_dir , leakage_powers);
generate_simulationScript( output_dir , assocs , nb_sets);


