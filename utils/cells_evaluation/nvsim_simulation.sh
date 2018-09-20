#!/bin/bash


if [ ! $# -eq 2 ]; then
	"No memory cells given";
	exit;
fi

declare -a cells=("sample_STTRAM" "SRAM");
declare -a param_size=(32 64 128 256 512 1024 2048 4096 8192);
declare -a param_assoc=(2);

pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	nvsim_dir="/home/gvaumour/Dev/apps/nvsim/"
else
	nvsim_dir="/home/gvaumour/proj/gvaumour/nvsim"
fi


output_dir=$2;

if [ ! -d $output_dir/nvsim_configs ]; then
	mkdir $output_dir/nvsim_configs;
fi
if [ ! -d $output_dir/nvsim_output ]; then
	mkdir $output_dir/nvsim_output;
fi

for cell in "${cells[@]}"
do
	for size in "${param_size[@]}"
	do
		echo "Simulation for "$cell", Size "$size"KB"

		file_config_name=$output_dir"/nvsim_configs/"$cell"_"$size"KB.cfg";
		file_output_name=$output_dir"/nvsim_output/"$cell"_"$size"KB.out";

		if [ ! -f $file_output_name ]; then

			echo "//Generated automatically from the script simulate.sh" > $file_config_name;
			echo "-Associativity (for cache only): 4" >> $file_config_name
			echo "-Capacity (KB): "$size >> $file_config_name
			
			if [ "$cell" == "sample_STTRAM" ]; then 
				echo "-MemoryCellInputFile: "$1 >> $file_config_name
			else
				echo "-MemoryCellInputFile: SRAM.cell" >> $file_config_name				
			fi
			
			cat ./default.cfg >> $file_config_name
		
			$nvsim_dir/nvsim $file_config_name > $file_output_name;
		fi
	done 
done
