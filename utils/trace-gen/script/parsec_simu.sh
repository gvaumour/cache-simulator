#!/bin/bash

my_dir="/home/gvaumour/Dev/parsec-3.0/pkgs/"

. ./parsec_config.rc

my_lib="/home/gvaumour/Dev/apps/pin/traces-pintools/obj-intel64/roeval.so"
pin_root="/home/gvaumour/Dev/apps/pin/pin-2.14-71313-gcc.4.4.7-linux/pin"
pin_flags=" -ifeellucky -injection child -follow-execv -t "$my_lib

output_dir="/home/gvaumour/Dev/apps/pin/traces-pintools/smalltraces_4threads/"
output_files="*.out"

cd ../
make release

#Record the trace configuration 
cd $output_dir
grep "define" ../src/roeval.cc > config.rc
echo -e "\n\n4 Threads used" >> config.rc

a=0;
for i in "${benchs[@]}"
do

	cd $output_dir
	if [ ! -d $i ]; then	
		 mkdir $i;
	fi 

	cd ${directories[$a]}
	echo "Running "$i
	echo "[---------- Beginning of output ----------]"
	$pin_root $pin_flags -- ${cmd_medium[$a]}
	echo "[---------- End of output ----------]"

	mv $output_files $output_dir/$i/


	((a++));
done 

