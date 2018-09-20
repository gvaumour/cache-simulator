#!/bin/bash

my_dir="/home/gvaumour/Dev/parsec-3.0/pkgs/"

. ./spec2006.rc

my_lib="../obj-intel64/roeval.so"
pin_root="/home/gvaumour/Dev/apps/pin/pin-2.14-71313-gcc.4.4.7-linux/pin"
pin_flags=" -ifeellucky -injection child -follow-execv -t "$my_lib

output_dir="/home/gvaumour/Dev/apps/pin/spec2006_traces/"
output_files="*.out"

grep "define" ../src/roeval.cc > $output_dir""config.rc

#Record the trace configuration 
cd $output_dir

a=0;
startInstru=false;

for i in "${benchs[@]}"
do	
	if [ $startInstru == true ]; then
	
		echo "Running "$i
		cd $output_dir
		if [ ! -d $i ]; then	
			 mkdir $i;
		fi 

		echo "Dir: "${directories[$a]}
		cd ${directories[$a]}
		echo "[---------- Beginning of output ----------]"
		echo "Command Line: "$pin_root" "$pin_flags" -- "${cmds[$a]};
		$pin_root $pin_flags -- ${cmds[$a]}
		echo "[---------- End of output ----------]"

		mv "memory_trace_"* $output_dir/$i/
	fi 
	((a++));
done 

