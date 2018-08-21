import os
import re
import sys

if len(sys.argv) != 3:
	print("Put the 2 results file to compare in arguments");
	exit();	


file1 = open(sys.argv[1], "r")
file2 = open(sys.argv[2], "r")
lines1 = file1.readlines();
lines2 = file2.readlines();

for i in range( len(lines1) ):
	
	split_line = lines1[i].split("\t");
	if len(split_line) < 2:
		print lines1[i][:-1];
		continue;
	 
	key = split_line[0];	
	value1 = split_line[1];
	split_line = lines2[i].split("\t");
	value2 = split_line[1];

	try:
		a = float(value1);
		b = float(value2);
		if a != 0:
			var = b / a;
		else:
			var = 0;
		print key + "\t" + str(a) + "\t" + str(b) + "\t" + '%1.1f' % float(100.0*var) +"%"; 
	except ValueError:
		print key + "\t" + value1[:-1] + "\t" + value2[:-1]; 
		    		
	

file1.close();
file2.close();
