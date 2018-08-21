#!/bin/bash

ENERGY_MODEL=/home/gvaumour/Dev/apps/energy_model/energy_model


#!/bin/bash

# loop & print a folder recusively,
print_folder_recurse() {
    for i in "$1"/*;do
       	filename=$(basename "$i")
       	
        if [ -d "$i" ];then
            print_folder_recurse "$i"
	elif [[ $filename == "results_1.out" ]]; then
		DIR=$(dirname "$i");
		miss_rate=$(grep "CacheLLC:MissRate"  $DIR/results_1.out | awk -F "\t" '{print $2}' )

		echo -e $DIR"\t"$miss_rate >> breakdown_dump.txt
        fi
        
    done
}


print_break_down() {
	path=$1
	rm breakdown.txt
	if [ -f breakdown_dump.txt ]; then
		rm breakdown_dump.txt
	fi
	print_folder_recurse $path

	python /home/gvaumour/Projects/DB-AMB_project/Utils/energy_model/generate_MR.py breakdown_dump.txt > breakdown.txt
#	rm breakdown_dump.txt
}


explo_dir()
{
	for i in ./*
	do
		if [ $i == "./bodytrack" ];then
			echo "Found simulation directory"
			print_break_down .
			return;
		fi
	done
	
	for i in ./*
	do
		if [ -d $i ];then
			cd $i
			echo "Going into the dir "$i
			explo_dir		
			cd ../
		fi  
	done
}


explo_dir
