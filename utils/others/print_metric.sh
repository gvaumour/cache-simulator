#!/bin/bash

if [[ $# -eq 2 ]]; then
	benchs=$1
else 
	echo "No set of benchmarks given, nothing done ... "
	exit;
fi 

if [[ $benchs == "parsec" ]];
then 
	declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")
else 
	declare -a benchs=("astar_0" "astar_1" "bzip2_0" "bzip2_1" "bzip2_2" "gcc_0" "gcc_1" "gcc_2" "gcc_3" "gcc_4" "gcc_5" "gcc_6" "gcc_7" "gcc_8" "gcc_9" "h264" "lbm" "libquantum" "mcf" "namd" "omnetpp" "povray" "sjeng_0" "soplex_0" "soplex_1" "sphinx3")
fi
	
#for a in */
#do
#	echo $a
#done
dir=$(realpath $2)

if [[ -f $dir/temp_file.txt ]];then
	rm $dir/temp_file.txt
fi 
touch $dir/temp_file.txt

for a in "${benchs[@]}"
do
	results="";
	for i in $dir/*
	do
		if [[ -d $i ]]; then 
			
			dummy=$(grep $a $i/breakdown.txt)
			if [ -z "$dummy" ]; then
				echo -e $a" 0" >> $dir/temp_file.txt
			else	
				echo $dummy >> $dir/temp_file.txt
			fi 

			cd ../ 
		fi 
	done
	echo -e "" >> $dir/temp_file.txt

done

python /home/gvaumour/Dev/apps/pin/cache-simulator/utils/others/generate_column.py $dir/temp_file.txt
