#!/bin/bash

declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")
#declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim")

ROOT_DIR="/home/gvaumour/Dev/apps/pin/"

output_dir=$ROOT_DIR"cache-simulator/output/test/"
cache_sim=$ROOT_DIR"cache-simulator/obj-intel64/roeval"
trace_dir=$ROOT_DIR"traces-pintools/smalltraces_4threads/"

output_files="config.ini results.out rap_predictor.out predictor.out"

cd $ROOT_DIR"cache-simulator/"
make release

test="blackscholes"

for i in "${benchs[@]}"
do
	if [ $i == $test ]; then

		cd $trace_dir
		if [ ! -d $i ]; then	
			echo "We didn't find the traces for "$i
			echo "Skipped !";
			continue;
		fi
		cd $i

		echo "Starting Benchmark "$i
		$cache_sim memory_trace*
	
		mv $output_files $output_dir/

		cd $output_dir/
		/home/gvaumour/Dev/apps/energy_model/energy_model > energy.out
	fi 
	
	((a++));
done 


