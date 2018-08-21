#!/bin/bash

pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	working_dir="/home/gvaumour/Dev/apps/pin/cache-simulator/";
else 
	working_dir="/home/gvaumour/proj/cache-simulator/";
fi

ENERGY_MODEL=$working_dir"utils/energy_modeling/energy_model.py"
AMAT=$working_dir"utils/energy_modeling/amat_computation.py"

print_folder_recurse() {
    for i in "$1"/*;do
       
        if [ -d "$i" ];then
            print_folder_recurse "$i"
	fi        
        if [[ $i == *"results_1.out"  ]]; then
            echo "Found simulation files: $i"
            DIR=$(dirname "$i")

		if [ -f $DIR/config_1_fix.ini ];then
			config_file=$DIR/config_1_fix.ini
		else
			config_file=$DIR/config_1.ini		
		fi
            python $ENERGY_MODEL $config_file $DIR/results_1.out > $DIR/energy.out
	    python $AMAT $config_file $DIR/results_1.out > $DIR/amat.out 
        fi
        
    done
}

if [[ $# -gt 0 ]]; then
	path=$1
	echo "base path: $path"
	print_folder_recurse $path
else 
	print_folder_recurse .
fi 

