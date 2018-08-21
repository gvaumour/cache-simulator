#!/bin/bash

#declare -a benchs=("blackscholes" "bodytrack" "canneal" "dedup" "facesim" "ferret" "fluidanimate" "freqmine" "raytrace" "streamcluster" "swaptions" "vips" "x264")
declare -a benchs=("freqmine")

pc_name=$(hostname -f)

if [ $pc_name == "gvaumour-HP-EliteBook-840-G3" ]; then 
	working_dir="/home/gvaumour/Dev/apps/pin";
	trace_dir="/home/gvaumour/Dev/apps/pin/parsec_traces"
elif [ $pc_name == "amstel.it.uu.se" ]; then 
	working_dir="/home/gvaumour/pin"
	trace_dir="/home/gvaumour/pin/parsec_traces"
else 
	working_dir="/home/gvaumour/proj";
	trace_dir="/proj/uppstore2017059/gvaumour/parsec_traces"
fi


output_dir=/home/gvaumour/uppstore/output/explo_signature
cache_sim=$working_dir/cache-simulator/obj-intel64/roeval_release

declare -a features=("Addr" , "MissPC" , "currentPC" , "MissCounter");
	
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

			script_name="./"$i"_"$feature1"_"$feature2".sh"

			echo -e "#!/bin/bash\n" > $script_name
			echo -e "a=0" >> $script_name
			echo -e "for trace in $trace_dir/$i/*.out" >> $script_name
			echo -e "do" >> $script_name
			echo -e "\tmkdir trace_\$a" >> $script_name
			echo -e "\tcd trace_\$a" >> $script_name
			echo -e "\t"$cache_sim" -n 1 -p Cerebron --PHC-Features "$feature1","$feature2" \$trace" >> $script_name
			echo -e "\tcd ../"  >> $script_name
			echo -e "\t((a++))" >> $script_name
			echo -e "done" >> $script_name	
			sbatch -A p2017001 -n 1 -p core -t 4:00:00 $script_name
		 	cd ../
		 	 
		done
	done
done 

