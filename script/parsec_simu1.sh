#!/bin/bash

#declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")
declare -a benchs=("freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")


output_dir="/home/gvaumour/Dev/apps/pin/cache-simulator/output/RAP/"
cache_sim="/home/gvaumour/Dev/apps/pin/cache-simulator/obj-intel64/roeval_release"
trace_dir="/home/gvaumour/Dev/apps/pin/traces-pintools/smalltraces_4threads/"
output_files="config.ini results.out rap_predictor.out predictor.out"

cd "/home/gvaumour/pin/cache-simulator/"
make release

for i in "${benchs[@]}"
do

	cd $output_dir
	if [ ! -d $i ]; then	
		 mkdir $i;
	fi

	cd $trace_dir
	if [ ! -d $i ]; then	
		echo "We didn't find the traces for "$i
		echo "Skipped !";
		continue;
	fi
	cd $i

	echo "Starting Benchmark "$i
	$cache_sim memory_trace*
	
	mv $output_files $output_dir/$i/

	cd $output_dir/$i
	/home/gvaumour/Dev/apps/energy_model/energy_model > energy.out

	((a++));
done 


