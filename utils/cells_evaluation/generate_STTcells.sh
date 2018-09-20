#!/bin/bash

declare -a params=(54 34 12.5 9);
#declare -a params=(54);

default_cell_parameters="-MemCellType: MRAM\n
-CellAspectRatio: 0.54\n
-ResistanceOn (ohm): 2000\n
-ResistanceOff (ohm): 1000\n
-ReadMode: current\n
-ReadVoltage (V): 0.25\n
-MinSenseVoltage (mV): 17\n
-ReadPower (uW): 30\n
-ResetMode: current\n
-ResetCurrent (uA): 34\n
-ResetPulse (ns): 10\n
-ResetEnergy (pJ): 1\n
-SetMode: current\n
-SetCurrent (uA): 80\n
-SetPulse (ns): 10\n
-SetEnergy (pJ): 1\n
-AccessType: CMOS\n
-VoltageDropAccessDevice (V): 0.15\n
-AccessCMOSWidth (F): 8\n"

pc_name=$(hostname -f)
if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	output_dir="/home/gvaumour/Dev/apps/pin/cache-simulator/utils/cells_evaluation/output";
else
	output_dir="/home/gvaumour/proj/gvaumour/pin/cells_evaluation/output";
fi


for param in "${params[@]}"
do

	echo "For param "$param"F";
	current_dir=$output_dir"/"$param"F"
	
	truc=$PWD;
	cd $output_dir
	mkdir $param"F"
	cd $truc
	
	file_cell=$current_dir"/sample_STTRAM_"$param"F.cell";

	echo -e "#File generated automatically with the generate_STTcells.sh script\n\n" > $file_cell 
	echo "-CellArea (F^2): "$param >> $file_cell
	echo -e $default_cell_parameters >> $file_cell

	python generate_configuration.py $file_cell $current_dir

done 


