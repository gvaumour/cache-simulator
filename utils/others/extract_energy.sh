#!/bin/bash

print_folder_recurse() {
    for i in $1/*
    do
        if [ -d "$i" ];then
            print_folder_recurse $i $2
        elif [[ $i == *"results_1.out"  ]]; then
		DIR=$(dirname "$i")
		truc1=$(grep -P "Leakage Power" $DIR/* |  awk -F "\t" '{print $2}' | column)		
		truc=$(grep -P "\tSRAM Read" $DIR/* -A 6 | awk -F "\t" '{print $3}' | column );

		echo -e $DIR"\t"$truc" "$truc1 >> $2	
        fi        
    done
}

print_break_down() {
	truc=$1
	dummy_file=$1/"breakdown_dump.txt"
	
	if [ -f $dummy_file ]; then
		rm $dummy_file
	fi
	touch $dummy_file
	
	print_folder_recurse $truc $dummy_file
	python /home/gvaumour/Dev/apps/pin/cache-simulator/utils/others/generate_breakdownV1.py $dummy_file > $1/breakdown.txt
}

print_break_down_spec() {
	truc=$1
	dummy_file=$1/"breakdown_dump.txt"
	
	if [ -f $dummy_file ]; then
		rm $dummy_file
	fi
	touch $dummy_file
	
	print_folder_recurse $truc $dummy_file
	python /home/gvaumour/Dev/apps/pin/cache-simulator/utils/others/generate_breakdownV1.py -spec2006 $dummy_file > $1/breakdown.txt
}

explo_dir()
{

	for i in ./*
	do
		if [ $i == "./bodytrack" ] ; then
			echo "Found PARSEC simulation directory"
			print_break_down .
			return;
		fi
		if [ $i == "./gcc_1" ] ; then
			echo "Found SPEC2006 simulation directory"
			print_break_down_spec . 
			return;
		fi
	done
	
	for i in ./*
	do
		if [ -d $i ];then
			cd $i
			echo "Going into the dir "$i
			explo_dir		
			cd ../
		fi  
	done
}


explo_dir
