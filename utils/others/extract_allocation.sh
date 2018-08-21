#!/bin/bash

print_folder_recurse() {
    for i in $1/*
    do
        if [ -d "$i" ];then
            print_folder_recurse $i $2
        elif [[ $i == *"results_1.out"  ]]; then
		DIR=$(dirname "$i");

		sram=$(grep "AllocateSRAM"  $DIR/results_1.out | awk -F "\t" '{print $2}' )
		nvm=$(grep "AllocateNVM"  $DIR/results_1.out | awk -F "\t" '{print $2}' )
		dram_access=$(grep "MainMem"  $DIR/results_1.out | awk -F "\t" '{print $2}' | awk '{sum += $1} END {print sum}')
		
		echo -e $DIR"\t"$sram" "$nvm" "$dram_access >> $2
#		echo -e $PWD"\t"$sram_read" "$sram_write" "$nvm_read" "$nvm_write
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
	
	python /home/gvaumour/Dev/apps/pin/cache-simulator/utils/others/generate_breakdown.py $dummy_file > $1/breakdown.txt
}

explo_dir()
{
	for i in ./*
	do
		if [ $i == "./bodytrack" ];then
			echo "Found simulation directory"
			print_break_down .
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
