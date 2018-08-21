#!/bin/bash

#declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")
declare -a benchs=("bodytrack");

pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	working_dir="/home/gvaumour/Dev/apps/pin";
	trace_dir="/home/gvaumour/Dev/apps/pin/parsec_traces"
elif [ $pc_name == "amstel.it.uu.se" ]; then 
	working_dir="/home/gvaumour/pin"
	trace_dir="/home/gvaumour/pin/parsec_traces"
else 
	working_dir="/home/gvaumour/proj/gvaumour/pin";
	trace_dir="/proj/uppstore2017059/gvaumour/parsec_traces"
fi


output_dir=$working_dir/output/explo_signature
app=$working_dir/cache-simulator/obj-intel64/roeval_release

END=15

for i in "${benchs[@]}"
do

	cd $output_dir
	if [ ! -d $i ]; then	
		 mkdir $i;
	fi
	cd $i	
		
	a=0;	
	for trace in $trace_dir/$i/*.out
	do
		mkdir "trace_"$a;
		cd "trace_"$a;
		
		echo "Exploration Benchmark "$i" trace "$trace

		for i in $(seq 3 $END); 
		do
			for j in $(seq $(($i+1)) $END); 
			do
				for k in $(seq $(($j+1)) $END); 
				do
		
					signature=$i"B"$j"B"$k"B";
					mkdir $signature
					cd $signature
					$app -n 1 --DBAMB-signature $signature $trace 
					cd ../ 
	
				done
			done 
		done
		cd ../
		((a++));
	done
done


