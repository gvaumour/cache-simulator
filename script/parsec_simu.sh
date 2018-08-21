#!/bin/bash

declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")

declare -a hashingFunctions=("2B13B14B" "2B13B14B" "12B13B15B" "2B3B12B" "2B3B4B" "2B3B4B" "1B4B5B" "0B4B14B" "4B13B15B" "1B2B3B" "3B14B15B" "3B4B13B" "1B4B13B" "1B4B13B" "1B4B13B" "1B4B13B" "12B13B14B" "12B13B14B" "12B13B14B" "12B13B15B" "3B13B15B" "12B13B15B" "6B14B15B" "12B14B15B" "6B14B15B" "13B14B15B" "5B12B15B" "4B13B14B" "1B3B4B" "2B4B13B" "2B4B5B" "2B3B4B" "2B3B4B" "2B3B4B" "2B3B4B" "2B4B15B" "4B12B15B" "0B13B15B" "4B5B15B" "3B4B15B" "3B4B15B" "4B13B14B" "4B13B14B" "4B13B14B" "4B13B14B" "4B13B14B" "2B4B15B" "2B13B14B" "2B12B15B" "1B12B14B" "1B4B13B" "2B11B12B" "2B3B12B" "2B12B14B" "2B12B14B" "2B3B13B")

output_dir="/home/gvaumour/Dev/apps/pin/output/hashingFunction"
cache_sim="/home/gvaumour/Dev/apps/pin/cache-simulator/obj-intel64/roeval_release"
trace_dir="/home/gvaumour/Dev/apps/pin/parsec_traces/"

index=0; 
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
		$cache_sim -n 1 -p DBAMB --DBAMB-signature ${hashingFunctions[$index]} $trace
		cd ../ 
		((a++));
		((index++))
	done

done 




