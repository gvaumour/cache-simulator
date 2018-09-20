#!/bin/bash

pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	ENERGY_MODEL=/home/gvaumour/Dev/apps/pin/cache-simulator/utils/energy_modeling/energy_model.py
else
	ENERGY_MODEL=home/gvaumour/proj/gvaumour/pin/cache-simulator/utils/energy_modeling/energy_model.py
fi


AMAT=

print_folder_recurse() {
    for i in "$1"/*;do
       
        if [ -d "$i" ];then
            print_folder_recurse "$i"
	fi        
        if [[ $i == *"results_1.out"  ]]; then
            echo "Found simulation files: $i"
            DIR=$(dirname "$i")

            python $ENERGY_MODEL $DIR/config_1.ini $DIR/results_1.out > $DIR/energy.out
	    python $AMAT $DIR/config_1.ini $DIR/results_1.out > $DIR/amat.out 
        fi
        
    done
}

if [[ $# -gt 0 ]]; then
	path=$1
	echo "base path: $path"
	print_folder_recurse $path
else 
	echo "No paths given , nothing done ... "
fi 

