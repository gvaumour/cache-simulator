#!/bin/bash

working_dir="";

pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	working_dir="/home/gvaumour/Dev/apps/pin/";
	energy_model="/home/gvaumour/Dev/apps/energy_model/energy_model";
else
	working_dir="/home/gvaumour/proj/gvaumour/pin/";
	energy_model="/home/gvaumour/apps/energy_model/energy_model";
fi

policy="DBAMB";

declare -a array_end_bits=(8 16 24 32 40 48 56 64);
declare -a array_begin_bits=(0 8 16 24 32 40 48 56);

cache_sim=$working_dir"cache-simulator/obj-intel64/roeval_release"
trace_dir="/proj/uppstore2017059/gvaumour/parsec_traces/"

declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")

output_dir="/proj/uppstore2017059/gvaumour/output/memory_region/"


index=0;

for begin_bits in "${array_begin_bits[@]}"
do
	cd $output_dir;
	mkdir $begin_bits
	
	current=$output_dir""$begin_bits"/"

	for i in "${benchs[@]}"
	do

		cd $current
		if [ ! -d $i ]; then	
			 mkdir $i;
		fi
		cd $i
		rm -rf * 

		echo "Current Dir:"$PWD
		echo "Starting Benchmark "$i

		script_name="./"$i"_"$policy.sh

		echo -e "#!/bin/bash\n" > $script_name
		echo -e "a=0" >> $script_name
		echo -e "for trace in $trace_dir/$i/*.out" >> $script_name
		echo -e "do" >> $script_name
		echo -e "\tmkdir trace_\$a" >> $script_name
		echo -e "\tcd trace_\$a" >> $script_name
		echo -e "\t"$cache_sim" -n 1 -p "$policy" --DBAMB-signature addr --DBAMB-addr-beginBits "${array_begin_bits[$index]}" --DBAMB-addr-endBits "${array_end_bits[$index]}" \$trace" >> $script_name
		echo -e "\tcd ../"  >> $script_name
		echo -e "\t((a++))" >> $script_name
		echo -e "done" >> $script_name	
		sbatch -A p2017001 -n 1 -p core -t 3:00:00 $script_name

	done 
	((index++));

done 


