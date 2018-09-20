#!/bin/bash


pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	cache_sim="/home/gvaumour/Dev/apps/pin/cache-simulator/obj-intel64/roeval_release";
	root_dir="/home/gvaumour/Dev/apps/pin/output"
	trace_dir="/home/gvaumour/Dev/apps/pin/parsec_traces/"
else
	cache_sim="/home/gvaumour/proj/gvaumour/pin/cache-simulator/obj-intel64/roeval_release";
	root_dir="/proj/uppstore2017059/gvaumour/output/"
	trace_dir="/proj/uppstore2017059/gvaumour/parsec_traces/"
fi

declare -a sram_assocs=
declare -a nvm_assocs=
nb_sets=

current=

declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")

index=0;
for sram_assoc in "${sram_assocs[@]}"
do
 	cd $current
 	config_name=""${sram_assocs[$index]}
 	
	mkdir $config_name

	output_dir=$current""$config_name


	for i in "${benchs[@]}"
	do

		cd $output_dir
		if [ ! -d $i ]; then	
			 mkdir $i;
		fi
		cd $i
		rm -rf * 

		echo "Starting Benchmark "$i

		script_name="./"$i"_"$policy.sh

		echo -e "#!/bin/bash\n" > $script_name
		echo -e "a=0" >> $script_name
		echo -e "for trace in $trace_dir/$i/*.out" >> $script_name
		echo -e "do" >> $script_name
		echo -e "\tmkdir trace_\$a" >> $script_name
		echo -e "\tcd trace_\$a" >> $script_name
		echo -e "\t"$cache_sim" -n 1 -p "$policy" --SRAM-assoc "${sram_assocs[$index]}" --NVM-assoc "${nvm_assocs[$index]}" --LLC-sets "$nb_sets" \$trace" >> $script_name
		echo -e "\tcd ../"  >> $script_name
		echo -e "\t((a++))" >> $script_name
		echo -e "done" >> $script_name	
		sbatch -A p2017001 -n 1 -p core -t 12:00:00 $script_name

	done 
	((index++));
done


