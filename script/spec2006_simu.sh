#!/bin/bash

declare -a benchs=("bzip2_0" "bzip2_1" "bzip2_2" "gcc_0" "gcc_1" "gcc_2" "gcc_3" "gcc_4" "gcc_5" "gcc_6" "gcc_7" "gcc_8" "gcc_9" "mcf" "milc" "namd" "soplex_0" "soplex_1" "povray" "sjeng_0" "soplex_1" "libquantum" "h264" "lbm" "omnetpp" "astar_0" "astar_1" "sphinx3" "specrand_0" "specrand_1")

declare -a predictors=("DB-AMB" "DB-A" "Saturation")

working_dir="/home/gvaumour/Dev/apps/pin/output/spec2006/"
cache_sim="/home/gvaumour/Dev/apps/pin/cache-simulator/obj-intel64/roeval_release"
trace_dir="/home/gvaumour/Dev/apps/pin/test_spec2006/"

for pred in "${predictors[@]}"
do
	cd $working_dir

	if [ ! -d $pred ]; then	
		 mkdir $pred;
	fi	

	output_dir=$working_dir""$pred
	
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
			$cache_sim -p $pred -n 1 $trace
			/home/gvaumour/Dev/apps/energy_model/energy_model > energy.out
			cd ../ 
			((a++));

		done

	done
done 


