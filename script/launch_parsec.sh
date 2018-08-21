#!/bin/bash

declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")

pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	working_dir="/home/gvaumour/Dev/apps/pin";
	trace_dir="/home/gvaumour/Dev/apps/pin/parsec_traces/"
else 
	working_dir="/home/gvaumour/proj/gvaumour/pin";
	trace_dir="/proj/uppstore2017059/gvaumour/output/"
fi


output_dir=$working_dir/cache-simulator/output/LLC_traces
app=$working_dir/cache-simulator/obj-intel64/roeval

for i in "${benchs[@]}"
do

	cd $output_dir
	if [ ! -d $i ]; then	
		 mkdir $i;
	fi
	cd $i

	echo "Starting Benchmark "$i
	
	a=0;	
	for trace in $trace_dir/$i/*.out
	do
		mkdir "trace_"$a;
		cd "trace_"$a;
		$app -n 1 --traceLLC -p LRU --SRAM-assoc 16 --NVM-assoc 0 $trace
		cd ../ 
		((a++));

	done

done 


